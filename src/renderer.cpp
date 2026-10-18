#include "renderer.hpp"
#include "vulkan_objects.hpp"
#include "tasks.hpp"
#include "utils.hpp"
#include "gui.hpp"

#include <cstdio>
#include <ctime>

struct Renderer {
	float value;
};

static SDL_AtomicInt is_shutting_down = SDL_AtomicInt(0);
static SDL_Thread* graphics_th;
static SDL_Mutex* graphics_mut;
static SDL_Thread* compute_th;
static SDL_Mutex* compute_mut;
static SDL_Thread* transfer_th;
static SDL_Mutex* transfer_mut;

Renderer renderer = {};

Image color_target = {};
Image gui_target = {};

void Renderer_Create(SDL_Window* window)
{
	VK_CHECK("volk initialize", volkInitialize());

	Instance_Create();
	Surface_Create(window);
	Instance_GetPhysicalDeviceData();
	Surface_PopulateData();
	Device_Create();
	Swapchain_Create();
	Allocator_Create();
	GUI_Create(window);

	GraphicsTask_Create(Swapchain_MaxFramesInFlight());
	ComputeTask_Create(2);
	TransferTask_Create(2);

	VkExtent2D surface_extent = Surface_Extent();
	color_target = Image_Create(
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VkExtent3D{ .width = surface_extent.width, .height = surface_extent.height, .depth = 1 },
		VK_IMAGE_USAGE_2_COLOR_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_2_TRANSFER_SRC_BIT_KHR,
		"color target"
	);
	gui_target = Image_Create(
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VkExtent3D{ .width = surface_extent.width, .height = surface_extent.height, .depth = 1 },
		VK_IMAGE_USAGE_2_COLOR_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_2_TRANSFER_SRC_BIT_KHR,
		"gui target"
	);
}

void Renderer_SetShuttingDown()
{
	SDL_SetAtomicInt(&is_shutting_down, 1);
}

static int GraphicsThread(void* ptr)
{
	srand(static_cast<uint32_t>(time(nullptr)) + static_cast<uint32_t>(SDL_GetThreadID(graphics_th)));
	while (SDL_GetAtomicInt(&is_shutting_down) != 1)
	{
		GraphicsTask_Begin();
		GraphicsTask_BeginRenderPass({ color_target }, VkRect2D{ .extent = Surface_Extent() });
		GraphicsTask_RenderGUI();
		GraphicsTask_EndRenderPass();
		GraphicsTask_End();

		SDL_LockMutex(graphics_mut);
		GraphicsTask_Submit({}, {});
		GraphicsTask_Present();
		SDL_UnlockMutex(graphics_mut);

		GraphicsTask_NextFrame();

		SDL_Delay(16);
	}

	return 0;
}

static int ComputeThread(void* ptr)
{
	while (SDL_GetAtomicInt(&is_shutting_down) != 1)
	{
		ComputeTask_Begin();
		ComputeTask_End();

		SDL_LockMutex(compute_mut);
		ComputeTask_Submit({}, {});
		SDL_UnlockMutex(compute_mut);

		ComputeTask_NextFrame();

		SDL_Delay(16);
	}

	return 0;
}

static int TransferThread(void* ptr)
{
	while (SDL_GetAtomicInt(&is_shutting_down) != 1)
	{
		TransferTask_Begin();
		TransferTask_End();

		SDL_LockMutex(transfer_mut);
		TransferTask_Submit({}, {});
		SDL_UnlockMutex(transfer_mut);

		TransferTask_NextFrame();

		SDL_Delay(16);
	}

	return 0;
}

void Renderer_StartJobs()
{
	graphics_mut = SDL_CreateMutex();
	compute_mut = SDL_CreateMutex();
	transfer_mut = SDL_CreateMutex();

	graphics_th = SDL_CreateThread(GraphicsThread, "Graphics Thread", nullptr);
	compute_th = SDL_CreateThread(ComputeThread, "Compute Thread", nullptr);
	transfer_th = SDL_CreateThread(TransferThread, "Transfer Thread", nullptr);
}

void Renderer_Event(SDL_Event* event)
{
	GUI_Event(event);
}

void Renderer_Destroy()
{
	SDL_WaitThread(transfer_th, nullptr);
	SDL_WaitThread(compute_th, nullptr);
	SDL_WaitThread(graphics_th, nullptr);

	SDL_DestroyMutex(transfer_mut);
	SDL_DestroyMutex(compute_mut);
	SDL_DestroyMutex(graphics_mut);

	VK_CHECK("wait device", vkDeviceWaitIdle(Device_Device()));

	Image_Destroy(gui_target);
	Image_Destroy(color_target);
	TransferTask_Destroy();
	ComputeTask_Destroy();
	GraphicsTask_Destroy();

	GUI_Destroy();
	Allocator_Destroy();
	Swapchain_Destroy();
	Device_Destroy();
	Surface_Destroy();
	Instance_Destroy();
}
