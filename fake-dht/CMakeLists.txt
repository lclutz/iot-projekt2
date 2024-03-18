cmake_minimum_required(VERSION 3.16)

project(fake-dht)

set(SOURCES
  fake-dht.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

find_package(date CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE date::date date::date-tz)

find_package(PahoMqttCpp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE PahoMqttCpp::paho-mqttpp3)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
