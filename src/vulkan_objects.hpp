#pragma once

#include <volk.h>
#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>

struct SDL_Window;

void Instance_Create();
void Instance_GetPhysicalDeviceData();
void Instance_Destroy();

void Surface_Create(SDL_Window* window);
void Surface_PopulateData();
void Surface_Destroy();

void Device_Create();
void Device_Destroy();

void Swapchain_Create();
void Swapchain_Destroy();

void Allocator_Create();
void Allocator_Destroy();

void GraphicsTask_Create();
void GraphicsTask_Begin();
void GraphicsTask_BeginRenderPass();
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

struct Image {
	VkImage image;
	VkImageView image_view;
	VkExtent3D extent;
	VmaAllocation allocation;
	VmaAllocationInfo2 allocation_info;
};

Image Image_Create();
void Image_Destroy(Image image);