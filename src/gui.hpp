#pragma once

#include <volk.h>

#include <SDL3/SDL.h>

void GUI_Create(SDL_Window* window);
void GUI_Event(SDL_Event* event);
void GUI_Render(VkCommandBuffer cmd_buff);
void GUI_Destroy();