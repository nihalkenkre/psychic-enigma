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
	Task_Start(graphics_task);
}

VkSemaphore GraphicsTask_Semaphore()
{
	return Task_Semaphore(graphics_task);
}

uint64_t GraphicsTask_SemaphoreValue()
{
	return Task_SemaphoreValue(graphics_task);
}

void GraphicsTask_ChangeImageLayout(
	VkImage image,
	VkImageLayout old_layout, VkImageLayout new_layout,
	VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
	VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask
)
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

	vkCmdPipelineBarrier2(Task_CommandBuffer(graphics_task), &dependency_info);
}

void GraphicsTask_BeginRenderPass(const std::vector<Image>& color_targets, const VkRect2D render_area)
{
	for (const auto& color_target : color_targets)
	{
		GraphicsTask_ChangeImageLayout(
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
	GraphicsTask_ChangeImageLayout(
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

void GraphicsTask_End(const Image& final_image)
{
	const VkAcquireNextImageInfoKHR acq_info = {
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = Swapchain_Swapchain(),
		.timeout = UINT64_MAX,
		.semaphore = Swapchain_AcqSigSem(),
		.deviceMask = 1,
	};

	VK_CHECK("acquire next image", vkAcquireNextImage2KHR(Device_Device(), &acq_info, &swapchain_image_index));

	VkImage swapchain_image = Swapchain_Image(swapchain_image_index);

	GraphicsTask_ChangeImageLayout(
		final_image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT
	);

	GraphicsTask_ChangeImageLayout(
		swapchain_image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT
	);

	const VkImageBlit2 regions[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
			.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1,
			},
			.srcOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}},
			.dstSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1
			},
			.dstOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}}
		}
	};

	const VkBlitImageInfo2 blit_info = {
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage = final_image.image,
		.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.dstImage = swapchain_image,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = _countof(regions),
		.pRegions = regions,
	};
	vkCmdBlitImage2(Task_CommandBuffer(graphics_task), &blit_info);
	GraphicsTask_ChangeImageLayout(
		swapchain_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0
	);

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
			.semaphore = Swapchain_AcqSigSem(),
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		}
	};

	Task_Submit(graphics_task, wait_sem_infos, {});
}

void GraphicsTask_Present()
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

