#pragma once

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