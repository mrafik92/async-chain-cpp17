# Async Chain
# Purpose
This project provides a modern, type-safe asynchronous chain implementation in C++17 inspired by promise chains and monadic pipelines. It enables composing sequences of asynchronous steps with robust error handling, retries, and delayed retries, all in a fluent and extensible style.

# Design & Structure
- **Template-Based:** The core `AsyncChain` class is fully generic, using templates for value and error types, as well as for each step in the chain.
- **Step Holders:** Each step (normal, retry, delayed, or error handler) is wrapped in a holder type that manages invocation and chaining logic.
- **Chaining API:** Steps are composed using methods like `then`, `thenWithRetry`, `thenWithRetryDelayed`, and `catchError`, each returning a new chain with the step appended.
- **Execution:** The chain is started with `finally`, which triggers the sequence and handles the final result.
- **Scheduler:** Delayed retries use a scheduler function, which can be customized for real or test environments.

# Strengths
- **Type Safety:** Extensive use of templates and static assertions ensures correct usage at compile time.
- **Composability:** Steps can be chained in a fluent, readable style, supporting both synchronous and asynchronous logic.
- **Error Handling:** Built-in support for error catching, retries, and delayed retries.
- **Extensibility:** New behaviors can be added by introducing new holder types or custom steps.
- **Modern C++:** Leverages C++20 features for concise, expressive, and safe code.

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
