#pragma once

#include <volk.h>
#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>

struct SDL_Window;

struct Image {
	VkImage image;
	VkImageView image_view;
	VkSampler sampler;
	VkExtent3D extent;
	VmaAllocation allocation;
	VmaAllocationInfo2 allocation_info;
};

void Instance_Create();
void Instance_GetPhysicalDeviceData();
void Instance_Destroy();

void Surface_Create(SDL_Window* window);
void Surface_PopulateData();
VkExtent2D Surface_Extent();
void Surface_Destroy();

void Device_Create();
VkDevice Device_Device();
void Device_Destroy();

void Swapchain_Create();
void Swapchain_Destroy();

void Allocator_Create();
void Allocator_Destroy();

void GraphicsTask_Create();
void GraphicsTask_Begin();
void GraphicsTask_BeginRenderPass(const std::vector<Image>& color_targets);
void GraphicsTask_EndRenderPass();
void GraphicsTask_End();
void GraphicsTask_Submit();
void GraphicsTask_Present();
void GraphicsTask_NextFrame();
void GraphicsTask_Destroy();

void TransferTask_Create();
void TransferTask_Begin();
void TransferTask_End();
void TransferTask_Submit();
void TransferTask_NextFrame();
void TransferTask_Destroy();

Image Image_Create(const VkFormat format, const VkExtent3D& extent, const VkImageUsageFlags2KHR usage, const std::string& name);
void Image_Destroy(Image image);