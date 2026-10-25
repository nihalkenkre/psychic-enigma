#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "utils.hpp"
#include "app.hpp"
#include "renderer.hpp"

static SDL_Window* window = nullptr;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
	SDL_CHECK(SDL_Init(SDL_INIT_VIDEO));

	SDL_Window* window = SDL_CreateWindow("Psychic-Enigma", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	SDL_CHECK(SDL_ShowWindow(window));

	App_Create(window);
	Renderer_Create(window);
	//Renderer_StartJobs();

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	if (event->type == SDL_EVENT_QUIT)
	{
		Renderer_SetShuttingDown();
		return SDL_APP_SUCCESS;
	}

	if (SDL_GetWindowFlags(window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN | SDL_WINDOW_OCCLUDED))
	{
		return SDL_APP_CONTINUE;
	}

	App_Event(event);

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
	Renderer_Render();
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	Renderer_Destroy();
	SDL_DestroyWindow(window);
	App_Destroy();
}

