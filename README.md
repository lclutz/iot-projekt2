![CI status](https://github.com/lclutz/cpp-template/actions/workflows/ci.yml/badge.svg)

# CPP Template

A template for cross-platform C++ projects using
[vcpkg](https://github.com/microsoft/vcpkg) and
[CMake](https://cmake.org/).

## Quick-Start

Prerequisits: [vcpkg](https://github.com/microsoft/vcpkg) installed and
              `VCPKG_ROOT` environment variable set up.

```shell
cmake --preset "default"
cmake --build --preset "debug"   # For debug build
cmake --build --preset "release" # For release build
```
