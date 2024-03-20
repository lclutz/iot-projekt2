// A Result<T> holds either a value of type T or an error of type Err

#pragma once

#include <string>
#include <variant>

struct Err : public std::string
{
};

template <typename V> using Result = std::variant<V, Err>;
