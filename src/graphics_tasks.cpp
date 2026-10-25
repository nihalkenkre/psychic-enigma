#include "graphics_tasks.hpp"
#include "compute_tasks.hpp"
#include "transfer_tasks.hpp"
#include "tasks.hpp"

#include "utils.hpp"
#include "vulkan_objects.hpp"
#include "gui.hpp"

static Task graphics_task = {};
static uint32_t swapchain_image_index = 0;

void GraphicsTask_Create(uint32_t max_frames_in_flight, const VkExtent3D& extent)
{
	Task_Create(graphics_task, Device_Graphics_Queue(), PhysicalDeviceData_GraphicsQueueFamilyIndex(), max_frames_in_flight, "graphics task");
}

void GraphicsTask_Begin()
{
	Task_Begin(graphics_task);
}

VkSemaphore GraphicsTask_Semaphore()
{
	return Task_Semaphore(graphics_task);
}

uint64_t GraphicsTask_SemaphoreValue()
{
	return Task_SemaphoreValue(graphics_task);
}

void GraphicsTask_BeginRenderPass(const std::vector<Image>& color_targets, const VkRect2D render_area)
{
	for (const auto& color_target : color_targets)
	{
		Task_ChangeImageLayout(
			graphics_task,
			color_target.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
		);
	}

	const VkClearValue clear_value = {
		.color = {
			.float32 = {(float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, 1},
		}
	};

	std::vector<VkRenderingAttachmentInfo> color_attachments;

	for (const auto& color_target : color_targets)
	{
		color_attachments.push_back({
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = color_target.image_view,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_value,
			});
	}

	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1,
		.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
		.pColorAttachments = color_attachments.data(),
	};

	vkCmdBeginRendering(Task_CommandBuffer(graphics_task), &rendering_info);
}

void GraphicsTask_BeginGUIRenderPass(const Image& gui_target, const VkRect2D render_area)
{
	Task_ChangeImageLayout(
		graphics_task,
		gui_target.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
	);

	const VkRenderingAttachmentInfo color_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = gui_target.image_view,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};

	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
	};

	vkCmdBeginRendering(Task_CommandBuffer(graphics_task), &rendering_info);
}

void GraphicsTask_RenderGUI()
{
	GUI_Render(Task_CommandBuffer(graphics_task));
}

void GraphicsTask_EndGUIRenderPass()
{
	vkCmdEndRendering(Task_CommandBuffer(graphics_task));
}

void GraphicsTask_EndRenderPass()
{
	vkCmdEndRendering(Task_CommandBuffer(graphics_task));
}

void GraphicsTask_End()
{
	Task_End(graphics_task);
}

void GraphicsTask_Submit()
{
	const std::vector<VkSemaphoreSubmitInfo> wait_sem_infos = {
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = ComputeTask_Semaphore(),
			.value = ComputeTask_SemaphoreValue(),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		},
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = GraphicsTask_Semaphore(),
			.value = GraphicsTask_SemaphoreValue(),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		},
	};

	const VkSemaphoreSubmitInfo sig_sem_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = GraphicsTask_Semaphore(),
		.value = GraphicsTask_SemaphoreValue() + 1,
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
	};

	//printf("graphics submit\n");
	//printf("compute sem value: %lld graphics sem value: %lld\n", wait_sem_infos.at(0).value, wait_sem_infos.at(1).value);

	Task_Submit(graphics_task, wait_sem_infos, { sig_sem_info });
}

void GraphicsTask_Present(const uint32_t swapchain_image_index)
{
	const VkSemaphore wait_semaphores[] = {
		ComputeTask_Semaphore(),
		GraphicsTask_Semaphore(),
	};

	const uint64_t wait_semaphore_values[] = {
		ComputeTask_SemaphoreValue(),
		GraphicsTask_SemaphoreValue(),
	};

	const VkSemaphoreWaitInfo wait_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = _countof(wait_semaphores),
		.pSemaphores = wait_semaphores,
		.pValues = wait_semaphore_values,
	};

	VK_CHECK("wait semaphores", vkWaitSemaphores(Device_Device(), &wait_info, UINT64_MAX));

	VkSemaphore prsnt_wait_sem = Swapchain_PrsntWaitSem();
	VkSwapchainKHR swapchain = Swapchain_Swapchain();

	const VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &prsnt_wait_sem,
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &swapchain_image_index,
	};

	//printf("graphics present\n");
	//printf("compute sem value: %lld graphics sem value: %lld\n", wait_semaphore_values[0], wait_semaphore_values[1]);
	VK_CHECK("queue present", vkQueuePresentKHR(graphics_task.queue, &present_info));
}

void GraphicsTask_NextFrame()
{
	Task_NextFrame(graphics_task);
	Swapchain_NextFrame();
}

void GraphicsTask_Destroy()
{
	Task_Destroy(graphics_task);
}

