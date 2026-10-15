#include "renderer.hpp"
#include "vulkan_objects.hpp"

#include "utils.hpp"
#include <cstdio>
#include <thread>

struct Renderer {
	float value;
};

static SDL_AtomicInt is_shutting_down = SDL_AtomicInt(0);
static SDL_Thread* graphics_th;
static SDL_Mutex* graphics_mut;
static SDL_Thread* transfer_th;
static SDL_Mutex* transfer_mut;

Renderer renderer = {};

void Renderer_Create(SDL_Window* window)
{
	VK_CHECK("volk initialize", volkInitialize());

	Instance_Create();
	Surface_Create(window);
	Instance_GetPhysicalDeviceData();
	Surface_PopulateData();
	Device_Create();
	Swapchain_Create();

	GraphicsTask_Create();
	TransferTask_Create();
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
		printf("graphics thread\n");

		SDL_LockMutex(graphics_mut);

		GraphicsTask_Begin();
		GraphicsTask_BeginRenderPass();
		GraphicsTask_EndRenderPass();
		GraphicsTask_End();
		GraphicsTask_Submit();
		GraphicsTask_Present();
		GraphicsTask_NextFrame();

		SDL_UnlockMutex(graphics_mut);

		SDL_Delay(16);
	}

	return 0;
}

static int TransferThread(void* ptr)
{
	srand(static_cast<uint32_t>(time(nullptr)) + static_cast<uint32_t>(SDL_GetThreadID(transfer_th)));
	while (SDL_GetAtomicInt(&is_shutting_down) != 1)
	{
		//printf("transfer thread\n");

		//SDL_LockMutex(transfer_mut);

		////TransferTask_Begin();
		////TransferTask_End();
		////TransferTask_Submit();
		////TransferTask_NextFrame();

		//SDL_UnlockMutex(transfer_mut);

		//SDL_Delay(100);
	}

	return 0;
}

void Renderer_StartJobs()
{
	graphics_mut = SDL_CreateMutex();
	transfer_mut = SDL_CreateMutex();

	graphics_th = SDL_CreateThread(GraphicsThread, "Graphics Thread", nullptr);
	transfer_th = SDL_CreateThread(TransferThread, "Transfer Thread", nullptr);
}

void Renderer_Destroy()
{
	SDL_WaitThread(graphics_th, nullptr);
	SDL_WaitThread(transfer_th, nullptr);

	SDL_DestroyMutex(graphics_mut);
	SDL_DestroyMutex(transfer_mut);

	VK_CHECK("wait device", vkDeviceWaitIdle(Device_Device()));

	TransferTask_Destroy();
	GraphicsTask_Destroy();

	Swapchain_Destroy();
	Device_Destroy();
	Surface_Destroy();
	Instance_Destroy();
}
