#pragma once

#include <vector>
#include "vulkan_objects.hpp"

void ComputeTask_Create(const std::vector<Image>& images, const Image& gui, const Image& final_image, uint32_t max_frames_in_flight);
void ComputeTask_Begin();
void ComputeTask_AddImages();
void ComputeTask_End();
void ComputeTask_Submit();
void ComputeTask_NextFrame();
void ComputeTask_Destroy();

VkSemaphore ComputeTask_Semaphore();
uint64_t ComputeTask_SemaphoreValue();

