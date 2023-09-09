cmake_minimum_required(VERSION 3.16)

set(SOURCES
  main.cpp
)

set(HEADERS
  pch.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()
