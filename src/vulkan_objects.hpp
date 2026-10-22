#pragma once

#include <volk.h>
#include <SDL3/SDL.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>

struct SDL_Window;

typedef struct Image {
	VkImage image;
	VkImageView image_view;
	VkSampler sampler;
	VkExtent3D extent;
	VmaAllocation allocation;
	VmaAllocationInfo2 allocation_info;
} Image;

typedef struct Buffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo2 allocation_info;
} Buffer;

void Instance_Create();
VkInstance Instance_Instance();
void Instance_GetPhysicalDeviceData();
void Instance_Destroy();

VkPhysicalDevice PhysicalDeviceData_PhysicalDevice();
uint32_t PhysicalDeviceData_GraphicsQueueFamilyIndex();
uint32_t PhysicalDeviceData_ComputeQueueFamilyIndex();
uint32_t PhysicalDeviceData_TransferQueueFamilyIndex();
VkPhysicalDeviceDescriptorBufferPropertiesEXT PhysicalDeviceData_DescBuffProps();

void Surface_Create(SDL_Window* window);
void Surface_PopulateData();
VkExtent2D Surface_Extent();
uint32_t Surface_MinImageCount();
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
uint32_t Swapchain_MaxFramesInFlight();
uint32_t Swapchain_ImageCount();
VkImage Swapchain_Image(uint32_t image_index);
void Swapchain_NextFrame();
void Swapchain_Destroy();

void Allocator_Create();
Image Allocator_CreateImage(const VkFormat format, const VkExtent3D& extent, const VkImageUsageFlags usage, const std::string& name);
void Allocator_DestroyImage(Image image);

Buffer Allocator_CreateBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage);
void Allocator_DestroyBuffer(Buffer buffer);

void Allocator_Destroy();
