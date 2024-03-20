#include <iostream>
#include <string>

#include "constants.h"
#include "result.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <date/date.h>

#include <mqtt/client.h>

#include <InfluxDBFactory.h>

static auto const ClientId = std::string{"ingress"};

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
    std::string clientId = ClientId;
    std::string topic = MqttTopic;
    int qos = MqttQos;

    friend std::ostream &operator<<(std::ostream &os, Config const &config)
    {
        os << "{"
            << "influxDbUrl:" << config.influxDbUrl << ","
            << "mqttUrl:" << config.mqttUrl << ","
            << "clientId:" << config.clientId << ","
            << "qos:" << config.qos << ","
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
    return !config.influxDbUrl.empty()
        && !config.mqttUrl.empty()
        && !config.topic.empty()
        && (0 <= config.qos && config.qos <= 3);
}

struct Measurement
{
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    float value;

    friend std::ostream &operator<<(std::ostream &os, Measurement const &measurement)
    {
        os << "{timestamp:" << date::format(TimeStampFormat, measurement.timestamp) << ",value:" << measurement.value << "}";
        return os;
    }
};

// Parse out the temperature and humidity measurements from an MQTT message payload
static Result<std::pair<Measurement, Measurement>> ParseMqttPayload(std::string const& payload)
{
    auto ptree = boost::property_tree::ptree{};
    try
    {
        auto stream = std::stringstream{payload};
        boost::property_tree::read_json(stream, ptree);
        auto timestamp = std::chrono::system_clock::time_point{};
        std::istringstream{ptree.get<std::string>("timestamp")}
            >> date::parse(TimeStampFormat, timestamp);
        auto const temperature = ptree.get<float>("temperature");
        auto const humidity = ptree.get<float>("humidity");
        return std::pair{Measurement{timestamp, temperature}, Measurement{timestamp, humidity}};
    }
    catch(boost::property_tree::json_parser_error const &e)
    {
        return Err{e.what()};
    }
    catch(boost::property_tree::ptree_error const &e)
    {
        return Err{e.what()};
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

    auto client = mqtt::client{config.mqttUrl, config.clientId, mqtt::create_options{MqttVersion}};

    auto const connOpts = mqtt::connect_options_builder()
        .mqtt_version(MqttVersion)
        .automatic_reconnect(std::chrono::seconds{2}, std::chrono::seconds{30})
        .clean_session(false)
        .finalize();

    auto db = influxdb::InfluxDBFactory::Get(config.influxDbUrl);
    try
    {
        db->createDatabaseIfNotExists();
    }
    catch (influxdb::InfluxDBException const &e)
    {
        std::cerr << "Failed to connect to InfluxDB: " << e.what() << std::endl;
        return 1;
    }

    try
    {
        std::cout << "Connecting to MQTT server..." << std::endl;
        auto const res = client.connect(connOpts);
        std::cout << "Connected." << std::endl;

        if (!res.is_session_present())
        {
            std::cout << "Subscribing to topic..." << std::endl;
            client.subscribe(config.topic, config.qos);
            std::cout << "Subscribed." << std::endl;

            std::cout << "Waiting on messages in " << config.topic << "..." << std::endl;
            while(true)
            {
                auto const msg = client.consume_message();
                if (nullptr != msg)
                {
                    auto const payload = msg->get_payload();
                    std::cout << "Message received: " << payload << std::endl;

                    auto const parseResult = ParseMqttPayload(payload);
                    if (std::holds_alternative<std::pair<Measurement, Measurement>>(parseResult))
                    {
                        auto const &[temp, humi] = std::get<std::pair<Measurement, Measurement>>(parseResult);
                        std::cout << "Temperature: " << temp << std::endl;
                        std::cout << "Humidity: " << humi << std::endl;

                        auto tempPoint = influxdb::Point{"temperature"}.setTimestamp(temp.timestamp).addField("value", temp.value);
                        db->write(std::move(tempPoint));

                        auto humiPoint = influxdb::Point{"humidity"}.setTimestamp(humi.timestamp).addField("value", humi.value);
                        db->write(std::move(humiPoint));
                    }
                    else
                    {
                        std::cerr << "Error parsing: " << std::get<Err>(parseResult) << std::endl;
                    }
                }
            }
        }
        else
        {
            std::cout << "Session already present. Skipping subscribe." << std::endl;
        }
    }
    catch (mqtt::exception const &e)
    {
        std::cerr << "MQTT error: " << e.what();
        return 1;
    }

    return 0;
}
