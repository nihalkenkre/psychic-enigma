#pragma once

#include <volk.h>
#include <SDL3/SDL.h>

struct SDL_Window;

void Instance_Create();
void Instance_Destroy();

void Surface_Create(SDL_Window* window);
void Surface_Destroy();