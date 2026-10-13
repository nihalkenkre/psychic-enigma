#include "renderer.hpp"
#include "vulkan_objects.hpp"

#include "utils.hpp"
#include <cstdio>

struct Renderer {

};

void Renderer_Create(SDL_Window* window)
{
	VK_CHECK("volk initialize", volkInitialize());

	Instance_Create();
	Surface_Create(window);
}

void Renderer_Destroy()
{
	Surface_Destroy();
	Instance_Destroy();
}
