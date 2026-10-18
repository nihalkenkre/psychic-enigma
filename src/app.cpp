#include "app.hpp"
#include "renderer.hpp"

struct App {
};

static App app;

void App_Create(SDL_Window* window)
{
}

void App_Event(SDL_Event* event)
{
	Renderer_Event(event);
}

void App_Destroy()
{
}