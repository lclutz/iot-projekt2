#pragma once

#include <chrono>
#include <future>

#include <SDL.h>

// Helper for turning a time_point into a float representing the number of
// seconds since epoch
template <typename T> static float TimePointToSeconds(std::chrono::time_point<T> const &tp)
{
    auto const milliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return milliSeconds / 1000.0f;
}

// Helper for logging errors
template <typename... Params> static void LogE(const char *const fmt, Params &&...params)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, fmt, std::forward<Params>(params)...);
}

// Helper for logging warnings
template <typename... Params> static void LogW(const char *const fmt, Params &&...params)
{
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, fmt, std::forward<Params>(params)...);
}

// Helper for logging infos
template <typename... Params> static void LogI(const char *const fmt, Params &&...params)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, fmt, std::forward<Params>(params)...);
}

// Wrapper for SDL functions returning error codes. Will log errors and crash
// the program in case of an error.
int SDL(int errorCode)
{
    if (0 > errorCode)
    {
        LogE("SDL Error: %s", SDL_GetError());
        exit(1);
    }
    return errorCode;
}

// Wrapper for SDL functions returning pointers to SDL data types. Will log
// errors and crash the program in case of an error.
template <typename T> T *SDL(T *const ptr)
{
    if (nullptr == ptr)
    {
        LogE("SDL Error: %s", SDL_GetError());
        exit(1);
    }
    return ptr;
}

// Helper for checking state of future
template <typename T> bool IsFutureDone(std::future<T> const& future)
{
    return future.valid() && future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}
