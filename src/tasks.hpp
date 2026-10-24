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
void GraphicsTask_End(const Image final_image);
void GraphicsTask_Submit();
void GraphicsTask_Present();
void GraphicsTask_NextFrame();
void GraphicsTask_Destroy();

VkSemaphore GraphicsTask_Semaphore();
uint64_t GraphicsTask_SemaphoreValue();

void ComputeTask_Create(const std::vector<Image>& images, const Image& gui, const Image& final_image, uint32_t max_frames_in_flight);
void ComputeTask_Begin();
void ComputeTask_AddImages();
void ComputeTask_End();
void ComputeTask_Submit();
void ComputeTask_NextFrame();
void ComputeTask_Destroy();

VkSemaphore ComputeTask_Semaphore();
uint64_t ComputeTask_SemaphoreValue();

void TransferTask_Create(uint32_t max_frames_in_flight);
void TransferTask_Begin();
void TransferTask_End();
void TransferTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos);
void TransferTask_NextFrame();
void TransferTask_Destroy();

VkSemaphore TransferTask_Semaphore();
uint64_t TransferTask_SemaphoreValue();
