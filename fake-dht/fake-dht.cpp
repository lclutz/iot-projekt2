#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "defer.h"

#include <date/date.h>

#include <mqtt/client.h>

static constexpr auto Qos = int{1};
static auto const ClientId = std::string{"fake-dht"};
static auto const Topic = std::string{"haus/temperatur"};

// Prints usage string
static void Usage(std::string const &executable)
{
    std::cerr << "Usage:\n\n"
              << executable << ": "
              << "--mqtt <MQTT Broker URL>"
              << "\n"
              << std::endl;
}

struct Config
{
    std::string mqttUrl;
    std::string clientId;
    std::string topic;
    int qos;

    friend std::ostream &operator<<(std::ostream &os, Config const &config)
    {
        os << "{mqttUrl: " << config.mqttUrl << ", clientId: " << config.clientId << "}";
        return os;
    }
};

// Turns command line arguments into a Config for the rest of the program to
// consume
static Config ParseConfig(int const argc, char *argv[])
{
    using namespace std::string_literals;

    auto config = Config{};
    config.clientId = ClientId;
    config.topic = Topic;
    config.qos = Qos;

    for (int i = 1; i < argc; ++i)
    {
        auto const arg = std::string{argv[i]};

        if ("--mqtt"s == arg && i + 1 < argc)
        {
            config.mqttUrl = argv[++i];
        }
    }

    return config;
}

// Check if all the neccessary fields were filled out
static bool ValidateConfig(Config const &config)
{
    return !config.mqttUrl.empty();
}

class MemoryPersistence : virtual public mqtt::iclient_persistence
{
  public:
    // "Open" the store
    void open(std::string const &, std::string const &) override
    {
        isOpen = true;
    }

    // Close the persistent store that was previously opened.
    void close() override
    {
        isOpen = false;
    }

    // Clears persistence, so that it no longer contains any persisted data.
    void clear() override
    {
        store.clear();
    }

    // Returns whether or not data is persisted using the specified key.
    bool contains_key(std::string const &key) override
    {
        return store.find(key) != store.end();
    }

    // Returns the keys in this persistent data store.
    mqtt::string_collection keys() const override
    {
        mqtt::string_collection ks;
        for (auto const &[k, _] : store)
        {
            ks.push_back(k);
        }
        return ks;
    }

    // Puts the specified data into the persistent store.
    void put(std::string const &key, std::vector<mqtt::string_view> const &bufs) override
    {
        std::stringstream stream;
        for (const auto &b : bufs)
        {
            stream << b;
        }
        store[key] = stream.str();
    }

    // Gets the specified data out of the persistent store.
    std::string get(std::string const &key) const override
    {
        if (auto p = store.find(key); p != store.end())
        {
            return p->second;
        }

        throw mqtt::persistence_exception();
    }

    // Remove the data for the specified key.
    void remove(std::string const &key) override
    {
        if (auto p = store.find(key); p != store.end())
        {
            store.erase(p);
        }

        throw mqtt::persistence_exception();
    }

  private:
    bool isOpen = false;
    std::map<std::string, std::string> store;
};

class UserCallback : public virtual mqtt::callback
{
    void connection_lost(const std::string &cause) override
    {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
        {
            std::cout << "\tcause: " << cause << std::endl;
        }
    }

    void delivery_complete(mqtt::delivery_token_ptr) override
    {
    }
};

static std::string GetRandomPayload()
{
    static constexpr auto temperature = 18.0f;
    static constexpr auto deltaTemperature = 3.0f;
    static constexpr auto humidity = 50.0f;
    static constexpr auto deltaHumidity = 5.0f;

    static auto dev = std::random_device{};
    static auto rng = std::default_random_engine{dev()};
    static auto dist = std::uniform_real<float>(-1.0f, 1.0f);

    auto timestamp = std::chrono::system_clock::now();

    auto temp = temperature + dist(rng) * deltaTemperature;
    auto humi = humidity + dist(rng) * deltaHumidity;

    std::stringstream stream;
    stream << "{"
           << "\"timestamp\":\"" << date::format("%F %T %Z", date::floor<std::chrono::milliseconds>(timestamp)) << "\","
           << "\"temperature\":" << temp << ","
           << "\"humidity\":" << humi
           << "}";

    return stream.str();
}

int main(int argc, char **argv)
{
    auto const config = ParseConfig(argc, argv);
    if (!ValidateConfig(config))
    {
        Usage(argv[0]);
        return 1;
    }

    std::cout << "Configuration: " << config << std::endl;

    std::cout << "Initializing..." << std::endl;
    auto persist = MemoryPersistence{};
    auto client = mqtt::client{config.mqttUrl, config.clientId, &persist};

    auto userCallback = UserCallback{};
    client.set_callback(userCallback);

    auto connOpts = mqtt::connect_options{};
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);
    std::cout << "Initialized." << std::endl;

    try
    {
        std::cout << "Connecting..." << std::endl;
        client.connect(connOpts);
        defer(client.disconnect());
        std::cout << "Connected." << std::endl;

        std::cout << "Sendingn messages..." << std::endl;
        while (true)
        {
            auto const payload = GetRandomPayload();
            auto pubmsg = mqtt::make_message(config.topic, payload);
            pubmsg->set_qos(config.qos);
            client.publish(pubmsg);
            std::cout << "Message sent to topic " << config.topic << ": " << payload << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
    catch (mqtt::persistence_exception const &e)
    {
        std::cerr << "Persistence Error: " << e.what() << std::endl;
        return 1;
    }
    catch (mqtt::exception const &e)
    {
        std::cerr << "MQTT Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
