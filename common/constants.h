#pragma once

#include <string>

#include <mqtt/create_options.h>

static constexpr auto MqttVersion = MQTTVERSION_5;
static constexpr auto MqttQos = int{1};
static auto const MqttTopic = std::string{"haus/dht"};
static auto const TimeStampFormat = std::string{"%F %T %Z"};
static auto const InfluxDbName = std::string{"sensor_data"};
