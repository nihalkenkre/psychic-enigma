#pragma once

#include <vector>
#include "vulkan_objects.hpp"

void GraphicsTask_Create(uint32_t max_frames_in_flight, const VkExtent3D& extent);
void GraphicsTask_Begin();
void GraphicsTask_BeginRenderPass(const std::vector<Image>& color_targets, const VkRect2D render_area);
void GraphicsTask_BeginGUIRenderPass(const Image& gui_target, const VkRect2D render_area);
void GraphicsTask_RenderGUI();
void GraphicsTask_EndGUIRenderPass();
void GraphicsTask_EndRenderPass();
void GraphicsTask_End();
void GraphicsTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos);
void GraphicsTask_Present();
void GraphicsTask_NextFrame();
void GraphicsTask_Destroy();

void ComputeTask_Create(uint32_t max_frames_in_flight);
void ComputeTask_Begin();
void ComputeTask_End();
void ComputeTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos);
void ComputeTask_NextFrame();
void ComputeTask_Destroy();

void TransferTask_Create(uint32_t max_frames_in_flight);
void TransferTask_Begin();
void TransferTask_End();
void TransferTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos);
void TransferTask_NextFrame();
void TransferTask_Destroy();

