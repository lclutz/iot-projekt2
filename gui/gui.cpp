#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "constants.h"
#include "defer.h"
#include "helpers.h"

#include <date/date.h>

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
    return {((rgba >> (8 * 3)) & 0xFF) / 255.0f, //
            ((rgba >> (8 * 2)) & 0xFF) / 255.0f, //
            ((rgba >> (8 * 1)) & 0xFF) / 255.0f, //
            ((rgba >> (8 * 0)) & 0xFF) / 255.0f};
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

struct TimeSeries
{
    std::vector<double> values;
    std::vector<double> timeStamps;

    void Insert(Measurement const &measurement)
    {
        values.emplace_back(static_cast<double>(measurement.value));
        auto const seconds = TimePointToSeconds(measurement.timeStamp);
        timeStamps.emplace_back(seconds);
        LogI("Seconds: %f", seconds);
    }

    void Insert(std::vector<Measurement> const &measurements)
    {
        for (auto const &measurement : measurements)
        {
            Insert(measurement);
        }
    }
};

// Wrapper for thread safe queries to InfluxDB
class Db
{
  public:
    ~Db()
    {
        // Grab a lock to make sure the Db object is not in use by another
        // thread when destructing
        auto const lg = std::lock_guard{m};
    }

    bool Connect(std::string const &url, std::string &errMsg) noexcept
    {
        auto const lg = std::lock_guard{m};
        try
        {
            db = influxdb::InfluxDBFactory::Get(url);
            db->createDatabaseIfNotExists();
            return true;
        }
        catch (influxdb::InfluxDBException const &e)
        {
            errMsg = e.what();
            return false;
        }
    }

    bool Query(std::string const &q, std::vector<influxdb::Point> &result,
               std::string &errMsg) noexcept
    {
        auto const lg = std::lock_guard{m};
        try
        {
            result = db->query(q);
            return true;
        }
        catch (influxdb::InfluxDBException const &e)
        {
            errMsg = e.what();
            return false;
        }
    }

  private:
    std::mutex m;
    std::shared_ptr<influxdb::InfluxDB> db;
};

struct State
{
    TimeSeries temperatureSeries;
    std::future<std::vector<Measurement>> temperatureFuture;
    std::chrono::system_clock::time_point temperatureCursor = std::chrono::system_clock::now();

    TimeSeries humiditySeries;
    std::future<std::vector<Measurement>> humidityFuture;
    std::chrono::system_clock::time_point humidityCursor = std::chrono::system_clock::now();

    Db db;
    bool showConnDialog = true;
    std::string influxDbUrl{"http://localhost:8086?db=" + InfluxDbName};

    bool fitToData = true;
};

static std::vector<Measurement> GetNewMeasurements(
    Db &db, std::string const &name, std::chrono::system_clock::time_point const &since)
{
    auto newMeasurements = std::vector<Measurement>{};

    auto stream = std::stringstream{};
    stream
        << "select * from " << name << " where time > "
        << std::chrono::duration_cast<std::chrono::nanoseconds>(since.time_since_epoch()).count();
    auto const query = stream.str();
    LogI("query: %s", query.c_str());
    auto errMsg = std::string{};
    auto points = std::vector<influxdb::Point>{};

    if (db.Query(query, points, errMsg))
    {
        for (auto point : points)
        {
            auto measurement = Measurement{};
            measurement.timeStamp = point.getTimestamp();
            measurement.value = std::stof(point.getFields().substr(6));
            newMeasurements.emplace_back(measurement);
        }
    }

    return newMeasurements;
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

                state.temperatureFuture =
                    std::async(std::launch::async, GetNewMeasurements, std::ref(state.db),
                               "temperature", std::ref(state.temperatureCursor));
                state.humidityFuture =
                    std::async(std::launch::async, GetNewMeasurements, std::ref(state.db),
                               "humidity", std::ref(state.humidityCursor));

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
        auto const measurements = state.temperatureFuture.get();
        if (!measurements.empty())
        {
            LogI("%lld new measurements", measurements.size());
            state.temperatureSeries.Insert(measurements);
            state.temperatureCursor = measurements.back().timeStamp;
        }
        state.temperatureFuture =
            std::async(std::launch::async, GetNewMeasurements, std::ref(state.db), "temperature",
                       std::ref(state.temperatureCursor));
    }

    if (IsFutureDone(state.humidityFuture))
    {
        auto const measurements = state.humidityFuture.get();
        if (!measurements.empty())
        {
            state.humiditySeries.Insert(measurements);
            state.humidityCursor = measurements.back().timeStamp;
        }
        state.humidityFuture =
            std::async(std::launch::async, GetNewMeasurements, std::ref(state.db), "humidity",
                       std::ref(state.humidityCursor));
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
            DrawTimeSeries("Temperature", "Temperature in °C", state.temperatureSeries,
                           TemperatureColor);

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
        SDL(SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x,
                               io.DisplayFramebufferScale.y));
        SDL(SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00));
        SDL(SDL_RenderClear(renderer));
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    return 0;
}
