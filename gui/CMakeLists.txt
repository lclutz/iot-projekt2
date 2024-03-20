cmake_minimum_required(VERSION 3.16)

project(gui)

set(SOURCES
  gui.cpp
)

set(HEADERS
  helpers.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/common)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

find_package(InfluxDB CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE InfluxData::InfluxDB)

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main SDL2::SDL2)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)

find_package(implot CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE implot::implot)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
