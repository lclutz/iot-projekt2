#include <iostream>
#include <string>

#include "constants.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <date/date.h>

#include <mqtt/client.h>

#include <InfluxDBFactory.h>

static auto const ClientId = std::string{"ingress"};

// Prints usage string
static void Usage(std::string const &executable)
{
    std::cerr << "Usage:\n\n"               //
              << executable << ": "         //
              << "--influx localhost:8086 " //
              << "--mqtt localhost:1883"    //
              << "\n"                       //
              << std::endl;
}

struct Config
{
    std::string influxDbUrl;
    std::string mqttUrl;
    std::string clientId = ClientId;
    std::string topic = MqttTopic;
    int qos = MqttQos;

    friend std::ostream &operator<<(std::ostream &os, Config const &config)
    {
        os << "{"                                         //
           << "influxDbUrl:" << config.influxDbUrl << "," //
           << "mqttUrl:" << config.mqttUrl << ","         //
           << "clientId:" << config.clientId << ","       //
           << "qos:" << config.qos << ","                 //
           << "}";

        return os;
    }
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
            auto stream = std::stringstream{};
            stream << "http://" << argv[++i] << "?db=" << InfluxDbName;
            config.influxDbUrl = stream.str();
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
    return !config.influxDbUrl.empty() //
           && !config.mqttUrl.empty()  //
           && !config.topic.empty()    //
           && (0 <= config.qos && config.qos <= 3);
}

struct Measurement
{
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    float value;

    friend std::ostream &operator<<(std::ostream &os, Measurement const &measurement)
    {
        os << "{"                                                                         //
           << "timestamp:" << date::format(TimeStampFormat, measurement.timestamp) << "," //
           << "value:" << measurement.value                                               //
           << "}";
        return os;
    }
};

using Temperature = Measurement;
using Humidity = Measurement;

// Parse out the temperature and humidity measurements from an MQTT message payload
static bool ParseMqttPayload(std::string const &payload, Temperature &temperature,
                             Humidity &humidity, std::string &errMsg) noexcept
{
    try
    {
        auto ptree = boost::property_tree::ptree{};
        auto stream = std::stringstream{payload};
        boost::property_tree::read_json(stream, ptree);

        auto timestamp = std::chrono::system_clock::time_point{};
        std::istringstream{ptree.get<std::string>("timestamp")} >>
            date::parse(TimeStampFormat, timestamp);

        temperature.timestamp = timestamp;
        humidity.timestamp = timestamp;

        temperature.value = ptree.get<float>("temperature");
        humidity.value = ptree.get<float>("humidity");

        return true;
    }
    catch (boost::property_tree::json_parser_error const &e)
    {
        errMsg = e.what();
        return false;
    }
    catch (boost::property_tree::ptree_error const &e)
    {
        errMsg = e.what();
        return false;
    }
}

int main(int argc, char *argv[])
{
    auto const config = ParseConfig(argc, argv);
    if (!ValidateConfig(config))
    {
        std::cerr << "Invalid config: " << config << std::endl;
        Usage(argv[0]);
        return 1;
    }

    std::cout << "Config: " << config << std::endl;

    try
    {
        auto client =
            mqtt::client{config.mqttUrl, config.mqttUrl, mqtt::create_options{MqttVersion}};

        auto const connOpts =
            mqtt::connect_options_builder()
                .mqtt_version(MqttVersion)
                .automatic_reconnect(std::chrono::seconds{2}, std::chrono::seconds{30})
                .clean_session(false)
                .finalize();

        auto db = influxdb::InfluxDBFactory::Get(config.influxDbUrl);
        db->createDatabaseIfNotExists();

        std::cout << "Connecting to MQTT server..." << std::endl;
        auto const res = client.connect(connOpts);
        std::cout << "Connected." << std::endl;

        if (!res.is_session_present())
        {
            std::cout << "Subscribing to topic..." << std::endl;
            client.subscribe(config.topic, config.qos);
            std::cout << "Subscribed." << std::endl;
        }
        else
        {
            std::cout << "Session already present. Skipping subscribe." << std::endl;
        }

        std::cout << "Waiting on messages in " << config.topic << "..." << std::endl;
        while (true)
        {
            auto const msg = client.consume_message();
            if (nullptr != msg)
            {
                auto const payload = msg->get_payload();
                std::cout << "Message received: " << payload << std::endl;

                auto temperature = Temperature{};
                auto humidity = Temperature{};
                auto errMsg = std::string{};
                if (ParseMqttPayload(payload, temperature, humidity, errMsg))
                {
                    std::cout << "Temperature: " << temperature << std::endl;
                    std::cout << "Humidity: " << humidity << std::endl;

                    auto tempPoint = influxdb::Point{"temperature"}
                                         .setTimestamp(temperature.timestamp)
                                         .addField("value", temperature.value);
                    db->write(std::move(tempPoint));

                    auto humiPoint = influxdb::Point{"humidity"}
                                         .setTimestamp(humidity.timestamp)
                                         .addField("value", humidity.value);
                    db->write(std::move(humiPoint));
                }
                else
                {
                    std::cerr << "Error parsing: " << errMsg << std::endl;
                }
            }
        }
    }
    catch (mqtt::exception const &e)
    {
        std::cerr << "MQTT error: " << e.what() << std::endl;
        return 1;
    }
    catch (influxdb::InfluxDBException const &e)
    {
        std::cerr << "InfluxDB error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
