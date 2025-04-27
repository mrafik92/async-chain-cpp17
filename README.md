# Async Chain

This is a modern C++ project using C++17 features, built with CMake. The project is set up for easy development and testing in a dev container with all necessary C/C++ tools pre-installed.

## Features
- C++17 standard
- CMake-based build system
- GoogleTest and GoogleMock for unit testing (fetched automatically)
- Ready-to-use development container with GCC, Clang, CMake, Make, Ninja, cppcheck, valgrind, lldb, gdb, vcpkg, and more

## Getting Started

### Prerequisites
- VS Code with Dev Containers support (or a compatible environment)
- Docker (if using Dev Containers)

### Build
```sh
cmake -S . -B build
cmake --build build
```

### Run
```sh
./build/AsyncChain
```

### Test
```sh
cmake --build build --target test_verbose
```
Or, to run all tests:
```sh
cd build
ctest --verbose
```

## Project Structure
- `main.cpp` – Main application source
- `CMakeLists.txt` – Build configuration
- `async.hpp` – Example/project header
- `build/` – Build output (created by CMake)

## Dev Container Tools
This project is ready for advanced C++ development with:
- GCC, G++, Clang, LLDB, LLVM, GDB
- CMake, Make, Ninja
- cppcheck, valgrind
- vcpkg for dependency management
- GoogleTest/GoogleMock for unit testing

## License
MIT or your preferred license.
