cmake_minimum_required(VERSION 3.11)

set(PROJECT "a3-motion-ui")
project(${PROJECT} VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig)
find_package(JUCE CONFIG REQUIRED)

add_subdirectory("src/a3-motion-engine")
add_subdirectory("src/a3-motion-ui")
add_subdirectory("src/a3-motion-pattern-gen")

set(TESTS_ENABLED TRUE CACHE BOOL "enable unit testing with GoogleTest")
if(TESTS_ENABLED)
# Without this CMake writes no CTestTestfile.cmake, and `ctest` in the build
# directory reports "No tests were found" however many gtest_discover_tests
# calls there are below. The suite was only ever reachable by running the
# test binary itself.
enable_testing()
add_subdirectory("src/a3-motion-tests")
endif()
