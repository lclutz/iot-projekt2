#pragma once

#include <utility>

#include <SDL.h>

template <typename... Params> static void LogE(const char *const fmt, Params &&...params)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, fmt, std::forward<Params>(params)...);
}

template <typename... Params> static void LogW(const char *const fmt, Params &&...params)
{
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, fmt, std::forward<Params>(params)...);
}

template <typename... Params> static void LogI(const char *const fmt, Params &&...params)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, fmt, std::forward<Params>(params)...);
}
