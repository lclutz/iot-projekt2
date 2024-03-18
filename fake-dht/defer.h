#pragma once

// Use defer(someFunction()); to queue up cleanup functions to be run at the
// end of the scope. Note that this implementation is semantically different
// from implementations if other languages e.g. Go where deferred functions
// are ran at the end of the function instead of the end of the scope.

template <typename F>
struct Defer
{
    Defer(F f) : f(f) {}
    ~Defer() { f(); }
    F f;
};

#define CONCAT0(a, b) a##b
#define CONCAT(a, b) CONCAT0(a, b)
#define defer(body) Defer CONCAT(defer, __LINE__)([&]() { body; })
