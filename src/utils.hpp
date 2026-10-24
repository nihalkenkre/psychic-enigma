#pragma once

#include <string>
#include <volk.h>

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

enum EDescriptorMode
{
    DescHeap,
    DescBuffer,
    DescIndex,
};

extern EDescriptorMode descriptor_mode;

inline static VkDeviceSize ALIGNED_SIZE(VkDeviceSize size, VkDeviceSize alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

void Utils_SetObjectName(VkDevice device, VkObjectType type, uint64_t handle, const std::string& name);