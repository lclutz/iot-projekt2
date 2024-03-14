#include <string>

#include <SDL.h>
#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "defer.h"

// Wrapper for SDL functions returning error codes. Will log errors and crash
// the program in case of an error.
static int SDL(int errorCode)
{
    if (0 > errorCode)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL Error: %s",
                     SDL_GetError());
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL Error: %s",
                     SDL_GetError());
        exit(1);
    }
    return ptr;
}

int main(int, char **)
{
    SDL(SDL_Init(SDL_INIT_VIDEO));
    defer(SDL_Quit());

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    static auto const title = std::string{"Visualisierung"};
    static constexpr int w = 800;
    static constexpr int h = 600;
    static constexpr Uint32 windowFlags = SDL_WINDOW_ALLOW_HIGHDPI;
    auto window =
        SDL(SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, w, h, windowFlags));
    defer(SDL_DestroyWindow(window));

    static constexpr Uint32 renderFlags =
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    static constexpr int deviceIndex = -1;
    auto renderer = SDL(SDL_CreateRenderer(window, deviceIndex, renderFlags));
    defer(SDL_DestroyRenderer(renderer));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    defer(ImGui::DestroyContext());

    auto &io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    defer(ImGui_ImplSDL2_Shutdown());

    ImGui_ImplSDLRenderer2_Init(renderer);
    defer(ImGui_ImplSDLRenderer2_Shutdown());

    auto shouldQuit = false;
    while (!shouldQuit)
    {
        static auto event = SDL_Event{};
        static constexpr int noMoreEvents = 0;
        while (noMoreEvents != SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                shouldQuit = true;
            }
        }
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();

        // ImGui::Begin(
        //     "Another Window",
        //     &show_another_window); // Pass a pointer to our bool variable
        //     (the
        //                            // window will have a closing button that
        //                            // will clear the bool when clicked)
        // ImGui::End();

        ImGui::Render();
        SDL(SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x,
                               io.DisplayFramebufferScale.y));
        SDL(SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00));
        SDL(SDL_RenderClear(renderer));
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    return 0;
}
