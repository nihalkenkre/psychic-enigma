#pragma once

#include <SDL3/SDL.h>

void Renderer_Create(SDL_Window* window);
void Renderer_StartJobs();
void Renderer_Event(SDL_Event* event);
void Renderer_SetShuttingDown();
void Renderer_Destroy();
