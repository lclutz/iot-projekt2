#pragma once

#include <string>

#include <mqtt/create_options.h>

[[maybe_unused]] constexpr auto MqttVersion = MQTTVERSION_5;
[[maybe_unused]] constexpr auto MqttQos = int{1};
[[maybe_unused]] static auto const MqttTopic = std::string{"haus/temperatur"};
[[maybe_unused]] static auto const TimeStampFormat = std::string{"%F %T %Z"};
