cmake_minimum_required(VERSION 3.16)

# Set the project name and version
project(AsyncChain VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_COLOR_DIAGNOSTICS ON)

# Add the async_chain library (header-only)
add_library(async_chain INTERFACE)
target_include_directories(async_chain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Link the async_chain library to the executable
add_executable(AsyncChain main.cpp)
target_link_libraries(AsyncChain PRIVATE async_chain pthread)

# Find the Threads package
find_package(Threads REQUIRED)

# Enable testing and add GoogleTest
include(CTest)
include(GoogleTest)

# Download and link GoogleTest (using FetchContent)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_async tests/test_async.cpp)
target_link_libraries(test_async PRIVATE async_chain gtest_main pthread)
gtest_discover_tests(test_async)