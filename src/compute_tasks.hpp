#pragma once

#include <vector>
#include "vulkan_objects.hpp"

void ComputeTask_Create(uint32_t max_frames_in_flight);
void ComputeTask_AddImages_Create(const std::vector<Image>& images, const Image& gui, const Image& final_image);
void ComputeTask_CopyImageToSwapchain_Create();
void ComputeTask_Begin();
void ComputeTask_AddImages();
uint32_t ComputeTask_SwapchainIndex();
void ComputeTask_CopyImageToSwapchain(const VkImage src_image);
void ComputeTask_End();
void ComputeTask_Submit();
void ComputeTask_NextFrame();
void ComputeTask_Destroy();

VkSemaphore ComputeTask_Semaphore();
uint64_t ComputeTask_SemaphoreValue();

