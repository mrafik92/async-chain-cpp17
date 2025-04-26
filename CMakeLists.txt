cmake_minimum_required(VERSION 3.16)

# Set the project name and version
project(AsyncChain VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(AsyncChain main.cpp)

# Find the Threads package
find_package(Threads REQUIRED)

# Add the executable
target_link_libraries(AsyncChain PRIVATE pthread)

## GoogleTest and GoogleMock via FetchContent
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE  # ← avoid the warning without touching policy

)
# For MSVC: prevent overriding parent project settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_custom_target(test_verbose
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running all tests verbosely"
)
# Test executable
# add_executable(MyTemplateClass_test tests/TestMyTemplateClass.cpp)
# target_compile_definitions(MyTemplateClass_test PRIVATE UNIT_TEST)
# target_link_libraries(MyTemplateClass_test PRIVATE gmock_main Threads::Threads)
# add_test(NAME MyTemplateClassTest COMMAND $<TARGET_FILE:MyTemplateClass_test>)

# # Concept tests executable
# add_executable(ConceptClass_test tests/TestConceptClass.cpp)
# target_compile_definitions(ConceptClass_test PRIVATE UNIT_TEST)
# target_link_libraries(ConceptClass_test PRIVATE gmock_main Threads::Threads)
# add_test(NAME ConceptClassTest COMMAND $<TARGET_FILE:ConceptClass_test>)