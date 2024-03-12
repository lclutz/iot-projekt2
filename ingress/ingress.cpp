#include <iostream>
#include <string>

#include <InfluxDBFactory.h>

// Prints usage string
static void Usage(std::string const &executable)
{
    std::cerr << "Usage:\n\n"
              << executable << ": "
              << "--influx <InfluxDB URL> "
              << "--mqtt <MQTT Broker URL>"
              << "\n" << std::endl;
}

struct Config
{
    std::string influxDbUrl;
    std::string mqttUrl;
};

// Turns command line arguments into a Config for the rest of the program to
// consume
static Config ParseConfig(int const argc, char *argv[])
{
    using namespace std::string_literals;

    auto config = Config{};

    for (int i = 1; i < argc; ++i)
    {
        auto const arg = std::string{argv[i]};

        if ("--influx"s == arg && i + 1 < argc)
        {
            config.influxDbUrl = argv[++i];
        }

        if ("--mqtt"s == arg && i + 1 < argc)
        {
            config.mqttUrl = argv[++i];
        }
    }

    return config;
}

// Checks if the user provided all neccessary configuration options
static bool ValidateConfig(Config const &config)
{
    return !(config.influxDbUrl.empty() || config.mqttUrl.empty());
}

int main(int argc, char *argv[])
{
    auto const config = ParseConfig(argc, argv);
    if (!ValidateConfig(config))
    {
        Usage(argv[0]);
        return 1;
    }

    std::cout << config.influxDbUrl << std::endl;
    std::cout << config.mqttUrl << std::endl;

    std::cout << "done." << std::endl;
    return 0;
}
