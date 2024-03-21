// A Result<T> holds either a value of type T or an error of type Err

#pragma once

#include <string>

template <typename T, typename E>
class Expected
{
public:
    using value_type = T;
    using error_type = E;

    constexpr Expected();
};

class Err : public std::string
{
};

template <typename V> using Result = Expected<V, Err>;
