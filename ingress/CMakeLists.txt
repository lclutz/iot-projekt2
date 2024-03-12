cmake_minimum_required(VERSION 3.16)

project(ingress)

set(SOURCES
  ingress.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

find_package(PahoMqttCpp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE PahoMqttCpp::paho-mqttpp3)

find_package(InfluxDB CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE InfluxData::InfluxDB)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
