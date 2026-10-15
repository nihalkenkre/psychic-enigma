#pragma once

struct SDL_Window;

void Renderer_Create(SDL_Window* window);
void Renderer_StartJobs();
void Renderer_SetShuttingDown();
void Renderer_Destroy();
