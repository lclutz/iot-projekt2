#pragma once

#include <random>

static float GetRandomNumber(float const min, float const max)
{
    static auto dev = std::random_device{};
    static auto rng = std::default_random_engine{dev()};
    auto dist = std::uniform_real<float>{min, max};
    return dist(rng);
}
