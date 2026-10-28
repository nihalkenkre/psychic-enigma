#pragma once

#include <vector>
#include <volk.h> 
#include <string>
#include "vulkan_objects.hpp"

struct Task {
	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkSemaphore> semaphores;
	std::vector<uint64_t> semaphore_values;

	VkQueue queue;
	uint32_t queue_family_index;

	uint32_t max_frames_in_flight;
	uint32_t current_frame;
};

void Task_Create(Task& task, VkQueue queue, const uint32_t queue_family_index, const uint32_t max_frames_in_flight, const std::string& name);
VkCommandBuffer Task_CommandBuffer(const Task& task);
VkSemaphore Task_Semaphore(const Task& task);
uint64_t& Task_SemaphoreValue(Task& task);
void Task_Begin(Task& task);
void Task_BeginRenderPass(const Task& task, const std::vector<Image>& color_targets, const VkRect2D render_area);
void Task_EndRenderPass(const Task& task);
void Task_BlitImageToSwapchain(const Task& task, const VkImage src_image, const VkImageLayout src_image_layout);
void Task_ChangeImageLayout(
	const Task& task,
	VkImage image,
	VkImageLayout old_layout, VkImageLayout new_layout,
	VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
	VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask
);
void Task_End(const Task& task);
void Task_Submit(Task& task, std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos);
void Task_Present(Task& task);
void Task_NextFrame(Task& task);
void Task_NextFrame(Task& task);
void Task_Destroy(Task task);
