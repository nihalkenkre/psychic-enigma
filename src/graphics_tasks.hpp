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
void GraphicsTask_Submit();
void GraphicsTask_Present();
void GraphicsTask_NextFrame();
void GraphicsTask_Destroy();

VkSemaphore GraphicsTask_Semaphore();
uint64_t GraphicsTask_SemaphoreValue();