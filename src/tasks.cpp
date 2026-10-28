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

struct AddImages {
	struct PushConstantRange {
		uint32_t image_width;
		uint32_t image_height;
		uint32_t image_count;
	};

	VkPipeline pipeline;

	// Heap
	struct DescHeap {
		Buffer buffer;
	};

	// Buffer
	//struct DescBuffer {
	//	Buffer buffer;
	//	VkPipelineLayout pipeline_layout;
	//	VkDescriptorSetLayout desc_set_layout;
	//	VkDeviceSize desc_set_layout_size;
	//};

	//// Indexing
	//struct DescIndexing {
	//	VkDescriptorPool desc_pool;
	//	VkDescriptorSet desc_set;
	//	VkPipelineLayout pipeline_layout;
	//	VkDescriptorSetLayout desc_set_layout;
	//};

	union {
		DescHeap desc_heap;
		//DescBuffer desc_buffer;
		//DescIndexing desc_indexing;
	};
};

static AddImages add_images = {};
static uint32_t swapchain_image_index = 0;

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

void Task_AddImages_Create(const Task& task)
{
	VkShaderModule shader_module = VK_NULL_HANDLE;

	std::string shader_path = std::string(SDL_GetBasePath()).append("shaders\\glsl\\add_images.comp.glsl.spv");

	SDL_IOStream* io_stream = SDL_IOFromFile(shader_path.c_str(), "rb");
	size_t code_size = 0;
	uint32_t* code = reinterpret_cast<uint32_t*>(SDL_LoadFile_IO(io_stream, &code_size, true));

	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(code_size),
		.pCode = code,
	};

	VK_CHECK("create add images shader module", vkCreateShaderModule(Device_Device(), &create_info, nullptr, &shader_module));
	SDL_free(code);

	const VkPipelineShaderStageCreateInfo stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shader_module,
		.pName = "main",
	};

	const VkPipelineCreateFlags2CreateInfo pipeline_create_flags = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
		.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
	};

	const VkComputePipelineCreateInfo pipeline_create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = &pipeline_create_flags,
		.stage = stage_create_info,
	};

	VK_CHECK("create add images pipeline", vkCreateComputePipelines(Device_Device(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &add_images.pipeline));

	vkDestroyShaderModule(Device_Device(), shader_module, nullptr);

	VkPhysicalDeviceDescriptorHeapPropertiesEXT desc_heap_props = PhysicalDeviceData_DescHeapProps();
	VkDeviceSize desc_size = ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment);
	VkDeviceSize total_heap_size = max(
		ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment) * 10,
		desc_heap_props.minResourceHeapReservedRange
	);

	add_images.desc_heap.buffer = Allocator_CreateBuffer(
		total_heap_size, VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT,
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		"add images descriptor heap"
	);
}

VkCommandBuffer Task_CommandBuffer(const Task& task)
{
	return task.command_buffers.at(task.current_frame);
}

VkSemaphore Task_Semaphore(const Task& task)
{
	return task.semaphores.at(task.current_frame);
}

uint64_t& Task_SemaphoreValue(Task& task)
{
	return task.semaphore_values.at(task.current_frame);
}

void Task_Begin(Task& task)
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

void Task_BeginRenderPass(const Task& task, const std::vector<Image>& color_targets, const VkRect2D render_area)
{
	for (const auto& color_target : color_targets)
	{
		Task_ChangeImageLayout(
			task,
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
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_value,
			}
			);
	}

	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1,
		.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
		.pColorAttachments = color_attachments.data(),
	};

	vkCmdBeginRendering(Task_CommandBuffer(task), &rendering_info);
}

void Task_EndRenderPass(const Task& task)
{
	vkCmdEndRendering(Task_CommandBuffer(task));
}

void Task_BeginGUIRenderPass(const Task& task, const Image& gui_target, const VkRect2D render_area)
{
	Task_ChangeImageLayout(
		task,
		gui_target.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
	);

	const VkClearValue clear_value = {
		.color = {
			.float32 = {0,0,0,0},
		}
	};

	const VkRenderingAttachmentInfo color_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = gui_target.image_view,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clear_value
	};

	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
	};

	vkCmdBeginRendering(Task_CommandBuffer(task), &rendering_info);
}

void Task_RenderGUI(const Task& task)
{
	GUI_Render(Task_CommandBuffer(task));
}

void Task_EndGUIRenderPass(const Task& task)
{
	vkCmdEndRendering(Task_CommandBuffer(task));
}

void Task_AddImages(const Task& task, const std::vector<Image>& images, const Image& gui_target, const Image& final_image)
{
	VkDeviceSize offset = 0;
	for (const Image image : images)
	{
		memcpy(
			reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset),
			image.descriptor.data(),
			image.descriptor.size()
		);
		offset += image.descriptor.size();
	}
	memcpy(
		reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset),
		gui_target.descriptor.data(),
		gui_target.descriptor.size()
	);
	offset += gui_target.descriptor.size();
	memcpy(
		reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset),
		final_image.descriptor.data(),
		final_image.descriptor.size()
	);
	offset += final_image.descriptor.size();
	const VkBindHeapInfoEXT bind_info = {
		.sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
		.heapRange = {
			.address = add_images.desc_heap.buffer.device_address,
			.size = add_images.desc_heap.buffer.allocation_info.allocationInfo.size,
		},
		.reservedRangeSize = add_images.desc_heap.buffer.allocation_info.allocationInfo.size,
	};

	const AddImages::PushConstantRange pc = {
		.image_width = Surface_Extent().width,
		.image_height = Surface_Extent().height,
		.image_count = static_cast<uint32_t>(images.size()) + 1 + 1,
	};

	const VkPushDataInfoEXT push_data = {
		.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
		.data = {
			.address = &pc,
			.size = sizeof(AddImages::PushConstantRange),
		}
	};

	VkCommandBuffer cmd_buff = Task_CommandBuffer(task);

	Task_ChangeImageLayout(
		task,
		final_image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT
	);

	vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, add_images.pipeline);
	vkCmdBindResourceHeapEXT(cmd_buff, &bind_info);
	vkCmdPushDataEXT(cmd_buff, &push_data);
	vkCmdDispatch(cmd_buff, Surface_Extent().width / 32 + 1, Surface_Extent().height / 32 + 1, 1);
}

void Task_BlitImageToSwapchain(const Task& task, const VkImage src_image, const VkImageLayout src_image_layout)
{
	const VkAcquireNextImageInfoKHR acq_info = {
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = Swapchain_Swapchain(),
		.timeout = UINT64_MAX,
		.semaphore = Swapchain_AcqSigSem(),
		.deviceMask = 1,
	};

	VkResult result = vkAcquireNextImage2KHR(Device_Device(), &acq_info, &swapchain_image_index);

	Task_ChangeImageLayout(
		task,
		src_image,
		src_image_layout, src_image_layout,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT
	);

	Task_ChangeImageLayout(
		task, Swapchain_Image(swapchain_image_index),
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR, 0,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT
	);

	const VkImageBlit2 region = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
		.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		},
		.srcOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}},
		.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		},
		.dstOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}},
	};

	VkBlitImageInfo2 blit_image_info = {
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage = src_image,
		.srcImageLayout = src_image_layout,
		.dstImage = Swapchain_Image(swapchain_image_index),
		.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.regionCount = 1,
		.pRegions = &region,
	};

	vkCmdBlitImage2(Task_CommandBuffer(task), &blit_image_info);
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
	Task_ChangeImageLayout(
		task, Swapchain_Image(swapchain_image_index),
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0
	);

	VK_CHECK("end task", vkEndCommandBuffer(Task_CommandBuffer(task)));
}

void Task_Submit(Task& task, std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos)
{
	wait_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		}
		);

	wait_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Swapchain_AcqSigSem(),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		}
		);

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
			.value = ++Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
		}
		);
	sig_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Swapchain_PrsntWaitSem(),
			.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		}
		);

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

void Task_Present(Task& task)
{
	VkSemaphore semaphore = Task_Semaphore(task);
	uint64_t semaphore_value = Task_SemaphoreValue(task);

	const VkSemaphoreWaitInfo wait_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &semaphore,
		.pValues = &semaphore_value,
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
	VK_CHECK("queue present", vkQueuePresentKHR(task.queue, &present_info));
}

void Task_NextFrame(Task& task)
{
	task.current_frame = (task.current_frame + 1) % task.max_frames_in_flight;
}

void Task_AddImages_Destroy()
{
	vkDestroyPipeline(Device_Device(), add_images.pipeline, nullptr);
	Allocator_DestroyBuffer(add_images.desc_heap.buffer);
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

