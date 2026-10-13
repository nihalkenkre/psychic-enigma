#include "app.hpp"
#include "renderer.hpp"

struct App {
	SDL_Window* window;
};

static App app;

void App_Create(SDL_Window* window)
{
	app.window = window;

	Renderer_Create(window);
}

SDL_Window* App_Window()
{
	return app.window;
}

void App_Destroy()
{
	Renderer_Destroy();
	SDL_DestroyWindow(app.window);
}