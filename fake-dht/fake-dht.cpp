#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

#include <mqtt/async_client.h>

static constexpr int QOS = 2;

static auto const TIMEOUT = std::chrono::seconds{10};

static constexpr int SESSION_EXPIRY_INTERVAL = 60; // in seconds
static constexpr int MESSAGE_EXPIRY_INTERVAL = 20; // in seconds

class SubscriptionCallback : public virtual mqtt::iaction_listener
{
    void on_failure(mqtt::token const &token) override
    {
        auto msg = std::stringstream{};
        msg << "Failure";

        auto const topic = token.get_topics();
        if (nullptr != topic && !topic->empty())
        {
            msg << " for topic " << (*topic)[0];
        }

        std::cout << msg.str() << std::endl;
    }

    void on_success(mqtt::token const &token) override
    {
        auto msg = std::stringstream{};
        msg << "Success";

        auto const topic = token.get_topics();
        if (nullptr != topic && !topic->empty())
        {
            msg << " for topic " << (*topic)[0];
        }

        std::cout << msg.str() << std::endl;
    }
};

class MqttClientCallback : public virtual mqtt::callback,
                           public virtual mqtt::iaction_listener
{
  public:
    explicit MqttClientCallback(mqtt::async_client &client)
        : client(client)
    {
    }

  private:
    mqtt::async_client &client;
    SubscriptionCallback subscriptionCallback;

    void on_failure(mqtt::token const &) override
    {
    }

    void on_success(mqtt::token const &) override
    {
    }

    void connection_lost(std::string const &cause) override
    {
        auto msg = std::stringstream{};
        msg << "Connection lost: " << cause;
        std::cout << msg.str() << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override
    {
        auto msg = std::stringstream{};
        msg << "Reveiced ACK for message id: "
            << (nullptr != token ? token->get_message_id() : -1)
            << " (only for QOS > 0)";
        std::cout << msg.str() << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override
    {
        auto stream = std::stringstream{};
        stream << "Message arrived"
               << "\n\tTopic:   " << msg->get_topic()
               << "\n\tPayload: " << msg->to_string();
        std::cout << stream.str() << std::endl;
    }

    void connected(std::string const &) override
    {
        auto msg = std::stringstream{};
        msg << "Connected: \n"
            << "Sending welcomemessage with QOS=" << QOS
            << " on topic 'hello'...";
        std::cout << msg.str() << std::endl;

        auto pubProps = mqtt::properties{};
        pubProps.add(
            {mqtt::property::MESSAGE_EXPIRY_INTERVAL, MESSAGE_EXPIRY_INTERVAL});

        auto const helloMsg = mqtt::message::create(
            "hello", "Hello from paho C++ client v5", QOS, true, pubProps);

        auto token = client.publish(helloMsg);
        if (mqtt::SUCCESS == token->get_return_code())
        {
            std::cout << "\t...OK, sent message with id: "
                      << token->get_message_id() << std::endl;
        }
        else
        {
            std::cout << "\t...NOK, message not sent." << std::endl;
        }

        std::cout << "Subscribing to topic 'hello'..." << std::endl;
        auto subToken =
            client.subscribe("hello", QOS, nullptr, subscriptionCallback);
        subToken->wait_for(1000); // just slow down for demo
        if (mqtt::SUCCESS == subToken->get_return_code())
        {
            std::cout << "\tOK, subscribed." << std::endl;
        }
        else
        {
            std::cout << "\tNOK, not subscribed." << std::endl;
        }
    }
};

struct Config
{
    std::string serverAddress;
    std::string clientId;
    std::string username;
    std::string password;
};

int main()
{
    auto const config = Config{
        "localhost:1883",
        "cpp-mqtt",
        "",
        "",
    };

    auto client = mqtt::async_client{config.serverAddress, config.clientId,
                                     mqtt::create_options{MQTTVERSION_5}};
    auto clientCallback = MqttClientCallback{client};
    client.set_callback(clientCallback);
    try
    {
        std::cout << "Connecting..." << std::endl;

        auto properties = mqtt::properties{};
        properties.add(
            {mqtt::property::SESSION_EXPIRY_INTERVAL, SESSION_EXPIRY_INTERVAL});

        auto const connOpts = mqtt::connect_options_builder{}
                                  .properties(properties)
                                  .user_name(config.username)
                                  .password(config.password)
                                  .mqtt_version(MQTTVERSION_5)
                                  .connect_timeout(TIMEOUT)
                                  .clean_start(true)
                                  .finalize();

        auto const connOk = client.connect(connOpts);
        std::cout << "Waiting for connections..." << std::endl;

        connOk->wait_for(TIMEOUT);
        if (!client.is_connected())
        {
            std::cerr << "Timeout expired." << std::endl;
            return 1;
        }

        // Block until the user quits
        while (std::tolower(std::cin.get()) != 'q')
            ;

        std::cout << "Disconnecting..." << std::endl;
        client.disconnect()->wait();
        std::cout << "Disconnected." << std::endl;
    }
    catch (mqtt::exception const &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
