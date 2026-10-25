#include "tasks.hpp"
#include "utils.hpp"
#include "vulkan_objects.hpp"
#include "gui.hpp"

#include <volk.h>
#include <vector>

enum ETaskType {
	Graphics,
	Transfer,
	Compute,
};

void Task_Create(Task& task, VkQueue queue, const uint32_t queue_family_index, const uint32_t max_frames_in_flight, const std::string& name)
{
	const VkSemaphoreTypeCreateInfo sem_type_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0,
	};

	const VkSemaphoreCreateInfo sem_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &sem_type_ci,
	};

	VkSemaphoreSignalInfo sem_sig_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
		.value = 1,
	};

	const VkCommandPoolCreateInfo cmd_pool_ci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queue_family_index,
	};

	VK_CHECK("create command pool", vkCreateCommandPool(Device_Device(), &cmd_pool_ci, nullptr, &task.command_pool));

#ifdef _DEBUG
	Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(task.command_pool), name + " command pool");
#endif

	task.max_frames_in_flight = max_frames_in_flight;
	task.command_buffers.resize(task.max_frames_in_flight);
	task.semaphores.resize(task.max_frames_in_flight);
	task.semaphore_values.resize(task.max_frames_in_flight, 1);
	task.queue = queue;

	const VkCommandBufferAllocateInfo allocate_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = task.command_pool,
		.commandBufferCount = task.max_frames_in_flight,
	};

	VK_CHECK("allocate command buffers", vkAllocateCommandBuffers(Device_Device(), &allocate_info, task.command_buffers.data()));

	for (uint32_t i = 0; i < task.max_frames_in_flight; ++i)
	{
		VK_CHECK("create semaphore", vkCreateSemaphore(Device_Device(), &sem_ci, nullptr, task.semaphores.data() + i));

#ifdef _DEBUG
		Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(task.semaphores.at(i)), name + " semaphore " + std::to_string(i));
		Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64_t>(task.command_buffers.at(i)), name + " command buffer " + std::to_string(i));
#endif
		sem_sig_info.semaphore = task.semaphores.at(i);
		VK_CHECK("signal semaphore", vkSignalSemaphore(Device_Device(), &sem_sig_info));
	}
}

VkCommandBuffer Task_CommandBuffer(const Task& task)
{
	return task.command_buffers.at(task.current_frame);
}

VkSemaphore Task_Semaphore(const Task& task)
{
	return task.semaphores.at(task.current_frame);
}

uint64_t Task_SemaphoreValue(const Task& task)
{
	uint64_t value = 0;

	VK_CHECK("get semaphore counter value", vkGetSemaphoreCounterValue(Device_Device(), Task_Semaphore(task), &value));
	return value;
}

void Task_Begin(const Task& task)
{
	VkSemaphore semaphore = Task_Semaphore(task);
	uint64_t value = Task_SemaphoreValue(task);

	const VkSemaphoreWaitInfo wait_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &semaphore,
		.pValues = &value,
	};

	VK_CHECK("wait semaphores", vkWaitSemaphores(Device_Device(), &wait_info, UINT64_MAX));

	const VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK("start task", vkBeginCommandBuffer(Task_CommandBuffer(task), &begin_info));
}

void Task_ChangeImageLayout(const Task& task, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask)
{
	const VkImageMemoryBarrier2 img_mem_barrs[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1,
},
}
	};

	const VkDependencyInfo dependency_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = _countof(img_mem_barrs),
		.pImageMemoryBarriers = img_mem_barrs,
	};

	vkCmdPipelineBarrier2(Task_CommandBuffer(task), &dependency_info);
}

void Task_End(const Task& task)
{
	VK_CHECK("end task", vkEndCommandBuffer(Task_CommandBuffer(task)));
}

void Task_Submit(Task& task, std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos)
{
	wait_sem_infos.push_back(
		VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		});

	const VkCommandBufferSubmitInfo cmd_buff_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = Task_CommandBuffer(task),
		}
	};

	sig_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = Task_SemaphoreValue(task) + 1,
			.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		});

	const VkSubmitInfo2 submit_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_sem_infos.size()),
			.pWaitSemaphoreInfos = wait_sem_infos.data(),
			.commandBufferInfoCount = _countof(cmd_buff_infos),
			.pCommandBufferInfos = cmd_buff_infos,
			.signalSemaphoreInfoCount = static_cast<uint32_t>(sig_sem_infos.size()),
			.pSignalSemaphoreInfos = sig_sem_infos.data(),
		},
	};

	VK_CHECK("queue submit", vkQueueSubmit2(task.queue, _countof(submit_infos), submit_infos, VK_NULL_HANDLE));
}

void Task_NextFrame(Task& task)
{
	task.current_frame = (task.current_frame + 1) % task.max_frames_in_flight;
}

void Task_Destroy(Task task)
{
	if (Device_Device() != VK_NULL_HANDLE)
	{
		if (task.command_pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(Device_Device(), task.command_pool, nullptr);
		}

		for (const auto sem : task.semaphores)
		{
			vkDestroySemaphore(Device_Device(), sem, nullptr);
		}
	}
}

