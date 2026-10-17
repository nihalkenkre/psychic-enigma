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

uint32_t PhysicalDeviceData_GraphicsQueueFamilyIndex();
uint32_t PhysicalDeviceData_ComputeQueueFamilyIndex();
uint32_t PhysicalDeviceData_TransferQueueFamilyIndex();

void Surface_Create(SDL_Window* window);
void Surface_PopulateData();
VkExtent2D Surface_Extent();
void Surface_Destroy();

void Device_Create();
VkDevice Device_Device();
VkQueue Device_Graphics_Queue();
VkQueue Device_Compute_Queue();
VkQueue Device_Transfer_Queue();
void Device_Destroy();

void Swapchain_Create();
VkSwapchainKHR Swapchain_Swapchain();
VkSemaphore Swapchain_AcqSigSem();
VkSemaphore Swapchain_PrsntWaitSem();
VkImage Swapchain_Image(uint32_t image_index);
uint32_t Swapchain_MaxFramesInFlight();
void Swapchain_NextFrame();
void Swapchain_Destroy();

void Allocator_Create();
void Allocator_Destroy();


Image Image_Create(const VkFormat format, const VkExtent3D& extent, const VkImageUsageFlags usage, const std::string& name);
void Image_Destroy(Image image);