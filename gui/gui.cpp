#include <chrono>
#include <future>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "defer.h"
#include "helpers.h"
#include "result.h"

// Library for talking to InfluxDB
#include <InfluxDBFactory.h>

// Library to abstract window creation on different platforms
#include <SDL.h>
#if !SDL_VERSION_ATLEAST(2, 0, 17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

// Library for immediate mode GUI creation
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <imgui_stdlib.h>

// Library for plotting graphs
#include <implot.h>

using TimeStamp = std::chrono::time_point<std::chrono::system_clock>;
using DataBase = std::unique_ptr<influxdb::InfluxDB>;

// Helper for defining ImGui colors as hex RGBA
constexpr ImVec4 RGBA(uint32_t const rgba)
{
    return {
        ((rgba >> (8 * 3)) & 0xFF) / 255.0f,
        ((rgba >> (8 * 2)) & 0xFF) / 255.0f,
        ((rgba >> (8 * 1)) & 0xFF) / 255.0f,
        ((rgba >> (8 * 0)) & 0xFF) / 255.0f
    };
}

static auto const Title = std::string{"Visualisierung"};
static auto const ConnectTitle = std::string{"Connect"};
static constexpr int Width = 800;
static constexpr int Height = 600;
static constexpr auto TemperatureColor{RGBA(0xC44E52FF)};
static constexpr auto HumidityColor{RGBA(0x55A868FF)};

struct Measurement
{
    TimeStamp timeStamp;
    float value;
};

// Produces random number between 0 and 1
static float GetRandomNumber()
{
    static auto dev = std::random_device{};
    static auto rng = std::default_random_engine{dev()};
    static auto dist = std::uniform_real<float>{0.0f, 1.0f};

    return dist(rng);
}

static TimeStamp GetTimeStamp()
{
    using namespace std::chrono;
    static auto const t0 = TimeStamp::clock::now();
    static auto const highResT0 = high_resolution_clock::now();
    auto const deltaTime = high_resolution_clock::now() - highResT0;
    auto const ms = duration_cast<seconds>(deltaTime);
    return t0 + ms;
}

static Result<std::vector<Measurement>> GetRandomMeasurements()
{
    std::this_thread::sleep_for(std::chrono::seconds{1});
    auto const timeStamp = GetTimeStamp();
    LogI("TimeStamp in Seconds: %f", TimePointToSeconds(timeStamp));
    return std::vector<Measurement>{{timeStamp, GetRandomNumber()}};
}

struct TimeSeries
{
    std::vector<float> values;
    std::vector<float> timeStamps;

    void Insert(Measurement const &measurement)
    {
        values.emplace_back(measurement.value);
        timeStamps.emplace_back(TimePointToSeconds(measurement.timeStamp));
    }

    void Insert(std::vector<Measurement> const &measurements)
    {
        for (auto const &measurement : measurements)
        {
            Insert(measurement);
        }
    }
};

struct DbConnectionState
{
    bool showDialog = true;
    std::string url{"http://localhost:8086?db=temperature_db"};
    std::string errorMsg;
    std::unique_ptr<influxdb::InfluxDB> db;
};

struct State
{
    TimeSeries temperatureSeries;
    std::future<Result<std::vector<Measurement>>> temperatureFuture;

    TimeSeries humiditySeries;
    std::future<Result<std::vector<Measurement>>> humidityFuture;

    DbConnectionState connection;
    bool fitToData = true;
};

// Connect to database or return an error message
// Stupid std::variant is forcing me to use a raw pointer here...
static Result<influxdb::InfluxDB *> Connect(std::string const &url)
{
    try
    {
        auto db = influxdb::InfluxDBFactory::Get(url).release();
        db->createDatabaseIfNotExists();
        return db;
    }
    catch (influxdb::InfluxDBException const &e)
    {
        return Err{e.what()};
    }
}

// Draw modal user dialog for connecting to the InfluxDB instance
static void DrawConnectDialog(State &state)
{
    if (state.connection.showDialog)
    {
        ImGui::OpenPopup("Connect");
    }

    if (ImGui::BeginPopupModal("Connect"))
    {
        ImGui::InputText("InfluxDB URL", &state.connection.url);

        if (ImGui::Button("Connect"))
        {
            LogI("Trying to connect to '%s'", state.connection.url.c_str());
            auto const connectResult = Connect(state.connection.url);
            if (std::holds_alternative<Err>(connectResult))
            {
                state.connection.errorMsg = std::get<Err>(connectResult);
                LogE("Failed to connect to %s: %s", state.connection.url.c_str(), state.connection.errorMsg.c_str());
            }
            else
            {
                state.connection.errorMsg.clear();
                state.connection.db =
                    std::unique_ptr<influxdb::InfluxDB>(std::get<influxdb::InfluxDB *>(connectResult));

                state.temperatureFuture = std::async(std::launch::async, GetRandomMeasurements);
                state.humidityFuture = std::async(std::launch::async, GetRandomMeasurements);

                state.connection.showDialog = false;
                ImGui::CloseCurrentPopup();
                LogI("Connected");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            state.connection.showDialog = false;
            ImGui::CloseCurrentPopup();
        }

        if (!state.connection.errorMsg.empty())
        {
            ImGui::TextWrapped("Failed to connect: %s", state.connection.errorMsg.c_str());
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
                state.connection.showDialog = true;
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
static void DrawTimeSeries(std::string const &title, std::string const &yLabel, TimeSeries &timeSeries,
                           ImVec4 const &color = IMPLOT_AUTO_COL)
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
        auto const result = state.temperatureFuture.get();
        if (std::holds_alternative<std::vector<Measurement>>(result))
        {
            state.temperatureSeries.Insert(std::get<std::vector<Measurement>>(result));
            state.temperatureFuture = std::async(std::launch::async, GetRandomMeasurements);
        }
        else
        {
            LogE("Failed to update temperature data: %s", std::get<Err>(result).c_str());
        }
    }

    if (IsFutureDone(state.humidityFuture))
    {
        auto const result = state.humidityFuture.get();
        if (std::holds_alternative<std::vector<Measurement>>(result))
        {
            state.humiditySeries.Insert(std::get<std::vector<Measurement>>(result));
            state.humidityFuture = std::async(std::launch::async, GetRandomMeasurements);
        }
        else
        {
            LogE("Failed to update humidity data: %s", std::get<Err>(result).c_str());
        }
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
            DrawTimeSeries("Temperature", "Temperature in °C", state.temperatureSeries, TemperatureColor);

            if (state.fitToData)
            {
                ImPlot::SetNextAxesToFit();
            }
            DrawTimeSeries("Humidity", "Humidity in %", state.humiditySeries, HumidityColor);

            ImPlot::EndSubplots();
        }

        ImGui::End();
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
    auto window = SDL(
        SDL_CreateWindow(Title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, windowFlags));
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

            if (event.type == SDL_WINDOWEVENT_RESIZED || event.type == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                auto width = int{0};
                auto height = int{0};
                SDL_GetWindowSize(window, &width, &height);

                auto const size = ImVec2{static_cast<float>(width), static_cast<float>(height)};
                ImGui::SetWindowSize(size);
            }
        }
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        UpdateData(state);
        RenderFrame(state);
        ImGui::Render();
        SDL(SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y));
        SDL(SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00));
        SDL(SDL_RenderClear(renderer));
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    return 0;
}
