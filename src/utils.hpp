#pragma once

#include <string>
#include <volk.h>

enum EDescriptorMode
{
    Heap,
    Buffer,
    Index,
};

extern EDescriptorMode descriptor_mode;

#define SDL_CHECK(result)                \
    if (!result)                         \
    {                                    \
        SDL_Log("%s\n", SDL_GetError()); \
    }

#define VK_CHECK(action, result)           \
    if (result != VK_SUCCESS)              \
    {                                      \
        printf("%s %d\n", action, result); \
    }

void Utils_SetObjectName(VkDevice device, VkObjectType type, uint64_t handle, const std::string& name);