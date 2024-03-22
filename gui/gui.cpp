#include <cmath>
#include <future>
#include <string>

#include "constants.h"
#include "db.h"
#include "db_reader.h"
#include "defer.h"
#include "gol.h"
#include "logging.h"

#include <SDL.h>
#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <imgui_stdlib.h>

#include <implot.h>

static auto const Title = std::string{"Visualisierung"};
static auto const ConnectTitle = std::string{"Connect"};
static constexpr int Width = 800;
static constexpr int Height = 600;

enum class Application
{
    VISUALISIERUNG,
    EASTER_EGG,
};

struct State
{
    Application app = Application::VISUALISIERUNG;
    bool fitToData = true;
    bool showConnDialog = true;

    std::string influxDbUrl{"http://localhost:8086?db=" + InfluxDbName};
    Db db;

    TimeSeries temperatures;
    DbReader temperatureReader{"temperature"};
    std::future<TimeSeries> temperatureFuture;

    TimeSeries humidities;
    DbReader humidityReader{"humidity"};
    std::future<TimeSeries> humidityFuture;

    gol::Gol gol;
};

// Helper for defining ImGui colors as hex RGBA
constexpr ImVec4 RGBA(uint32_t const rgba)
{
    return {((rgba >> (8 * 3)) & 0xFF) / 255.0f, //
            ((rgba >> (8 * 2)) & 0xFF) / 255.0f, //
            ((rgba >> (8 * 1)) & 0xFF) / 255.0f, //
            ((rgba >> (8 * 0)) & 0xFF) / 255.0f};
}

static constexpr auto TemperatureColor{RGBA(0xC44E52FF)};
static constexpr auto HumidityColor{RGBA(0x55A868FF)};

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

// Draw modal user dialog for connecting to the InfluxDB instance
static void DrawConnectDialog(State &state)
{
    static auto errorMsg = std::string{};
    if (state.showConnDialog)
    {
        ImGui::OpenPopup("Connect");
    }

    if (ImGui::BeginPopupModal("Connect"))
    {
        ImGui::InputText("InfluxDB URL", &state.influxDbUrl);

        if (ImGui::Button("Connect"))
        {
            LogI("Trying to connect to '%s'", state.influxDbUrl.c_str());
            if (state.db.Connect(state.influxDbUrl, errorMsg))
            {
                errorMsg.clear();

                state.temperatureFuture = std::async(
                    std::launch::async, std::ref(state.temperatureReader), std::ref(state.db));
                state.humidityFuture = std::async(
                    std::launch::async, std::ref(state.humidityReader), std::ref(state.db));

                state.showConnDialog = false;
                ImGui::CloseCurrentPopup();
                LogI("Connected");
            }
            else
            {
                LogE("Failed to connect to %s: %s", state.influxDbUrl.c_str(), errorMsg.c_str());
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            state.showConnDialog = false;
            ImGui::CloseCurrentPopup();
        }

        if (!errorMsg.empty())
        {
            ImGui::TextWrapped("Failed to connect: %s", errorMsg.c_str());
        }

        ImGui::EndPopup();
    }
}

// Draw the main window menu
static void DrawMainMenuBar(State &state)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Connect to database"))
            {
                state.showConnDialog = true;
            }

            if (ImGui::MenuItem("Exit"))
            {
                auto event = SDL_Event{SDL_QUIT};
                SDL_PushEvent(&event);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::Checkbox("Fit to data", &state.fitToData);

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// Graph time series for a measurement
static void DrawTimeSeries(std::string const &title, std::string const &yLabel,
                           TimeSeries &timeSeries, ImVec4 const &color = IMPLOT_AUTO_COL)
{
    if (ImPlot::BeginPlot(title.c_str()))
    {
        ImPlot::SetupAxes("Timestamp", yLabel.c_str());
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);

        auto &style = ImPlot::GetStyle();
        style.UseLocalTime = true;
        style.Marker = ImPlotMarker_Circle;

        ImPlot::SetNextLineStyle(color);

        ImPlot::PlotLine(title.c_str(), timeSeries.timeStamps.data(), timeSeries.values.data(),
                         static_cast<int>(timeSeries.timeStamps.size()));

        ImPlot::EndPlot();
    }
}

// Update time data from database
static void UpdateData(State &state)
{
    if (IsFutureDone(state.temperatureFuture))
    {
        auto const temperatures = state.temperatureFuture.get();
        state.temperatures.Append(temperatures);
        state.temperatureFuture =
            std::async(std::launch::async, std::ref(state.temperatureReader), std::ref(state.db));
    }

    if (IsFutureDone(state.humidityFuture))
    {
        auto const humidities = state.humidityFuture.get();
        state.humidities.Append(humidities);
        state.humidityFuture =
            std::async(std::launch::async, std::ref(state.humidityReader), std::ref(state.db));
    }
}

// Display the current input
static void RenderFrame(State &state)
{
    DrawMainMenuBar(state);
    DrawConnectDialog(state);

    auto *const viewPort = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(viewPort->WorkSize);
    ImGui::SetNextWindowPos(viewPort->WorkPos);

    static auto const windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize;
    if (ImGui::Begin(Title.c_str(), nullptr, windowFlags))
    {

        if (ImPlot::BeginSubplots("Sensor data", 2, 1, ImGui::GetContentRegionAvail()))
        {
            if (state.fitToData)
            {
                ImPlot::SetNextAxesToFit();
            }
            DrawTimeSeries("Temperature", "Temperature in °C", state.temperatures,
                           TemperatureColor);

            if (state.fitToData)
            {
                ImPlot::SetNextAxesToFit();
            }
            DrawTimeSeries("Humidity", "Humidity in %", state.humidities, HumidityColor);

            ImPlot::EndSubplots();
        }

        ImGui::End();
    }
}

static void RenderGol(SDL_Renderer *const renderer, gol::Gol &gol, float const windowWidth,
                      float const windowHeight)
{
    auto const cellHeight = windowHeight / gol::Height;
    auto const cellWidth = windowWidth / gol::Width;

    SDL_SetRenderDrawColor(renderer, 0x56, 0x52, 0x6e, 0xff);
    for (size_t y = 1; y <= gol::Height; ++y)
    {
        auto const x1 = 0;
        auto const y1 = static_cast<int>(std::round(0.0f + y * cellHeight));
        auto const x2 = static_cast<int>(std::round(windowWidth));
        auto const y2 = y1;
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }

    for (size_t x = 1; x <= gol::Width; ++x)
    {
        auto const x1 = static_cast<int>(std::round(0.0f + x * cellWidth));
        auto const y1 = 0;
        auto const x2 = x1;
        auto const y2 = static_cast<int>(std::round(windowHeight));
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }

    SDL_SetRenderDrawColor(renderer, 0xea, 0x9a, 0x97, 0xff);
    const auto cells = gol.GetCells();
    for (size_t y = 0; y < gol::Height; ++y)
    {
        for (size_t x = 0; x < gol::Width; ++x)
        {
            if (cells.at(y * gol::Width + x))
            {
                SDL_Rect rect;
                rect.x = static_cast<int>(std::round(x * cellWidth));
                rect.y = static_cast<int>(std::round(y * cellHeight));
                rect.w = static_cast<int>(std::ceil(cellWidth));
                rect.h = static_cast<int>(std::ceil(cellHeight));
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
}

int main(int, char **)
{
    SDL(SDL_Init(SDL_INIT_VIDEO));
    defer(SDL_Quit());

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    static constexpr Uint32 windowFlags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    auto window = SDL(SDL_CreateWindow(Title.c_str(), SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED, Width, Height, windowFlags));
    defer(SDL_DestroyWindow(window));

    static constexpr Uint32 renderFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    static constexpr int deviceIndex = -1;
    auto renderer = SDL(SDL_CreateRenderer(window, deviceIndex, renderFlags));
    defer(SDL_DestroyRenderer(renderer));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    defer(ImGui::DestroyContext());

    ImPlot::CreateContext();
    defer(ImPlot::DestroyContext());

    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    defer(ImGui_ImplSDL2_Shutdown());

    ImGui_ImplSDLRenderer2_Init(renderer);
    defer(ImGui_ImplSDLRenderer2_Shutdown());

    auto state = State{};

    auto shouldQuit = false;
    while (!shouldQuit)
    {
        auto event = SDL_Event{};
        static constexpr int noMoreEvents = 0;
        while (noMoreEvents != SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                shouldQuit = true;
            }

            if (event.type == SDL_KEYDOWN)
            {
                if (SDLK_ESCAPE == event.key.keysym.sym)
                {
                    switch (state.app)
                    {
                    case Application::VISUALISIERUNG: {
                        state.app = Application::EASTER_EGG;

                        state.gol.Clear();
                        state.gol.Set(5, 11, true);
                        state.gol.Set(6, 12, true);
                        state.gol.Set(6, 13, true);
                        state.gol.Set(5, 13, true);
                        state.gol.Set(4, 13, true);
                    }
                    break;
                    case Application::EASTER_EGG: {
                        state.app = Application::VISUALISIERUNG;
                    }
                    break;
                    }
                }
            }

            if (event.type == SDL_WINDOWEVENT_RESIZED || event.type == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                auto width = int{0};
                auto height = int{0};
                SDL_GetWindowSize(window, &width, &height);
                ImGui::SetWindowSize({static_cast<float>(width), static_cast<float>(height)});
            }
        }
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (Application::VISUALISIERUNG == state.app)
        {
            UpdateData(state);
            RenderFrame(state);
        }

        ImGui::Render();
        SDL(SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x,
                               io.DisplayFramebufferScale.y));
        SDL(SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00));
        SDL(SDL_RenderClear(renderer));
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

        if (Application::EASTER_EGG == state.app)
        {

            using namespace std::chrono;
            static auto lastUpdate = system_clock::now();

            if (system_clock::now() - lastUpdate >= system_clock::duration{milliseconds{100}})
            {
                lastUpdate = system_clock::now();
                state.gol.Update();
            }

            auto width = int{0};
            auto height = int{0};
            SDL_GetWindowSize(window, &width, &height);
            RenderGol(renderer, state.gol, static_cast<float>(width), static_cast<float>(height));
        }

        SDL_RenderPresent(renderer);
    }

    return 0;
}
