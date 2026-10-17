#pragma once

#include <vector>
#include "vulkan_objects.hpp"

void GraphicsTask_Create(uint32_t max_frames_in_flight);
void GraphicsTask_Begin();
void GraphicsTask_BeginRenderPass(const std::vector<Image>& color_targets, const VkRect2D render_area);
void GraphicsTask_EndRenderPass();
void GraphicsTask_End();
void GraphicsTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos);
void GraphicsTask_Present();
void GraphicsTask_NextFrame();
void GraphicsTask_Destroy();

void TransferTask_Create(uint32_t max_frames_in_flight);
void TransferTask_Begin();
void TransferTask_End();
void TransferTask_Submit();
void TransferTask_NextFrame();
void TransferTask_Destroy();

