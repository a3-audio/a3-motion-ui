cmake_minimum_required(VERSION 3.11)

set(PROJECT "a3-motion-ui")
project(${PROJECT} VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)

find_package(JUCE CONFIG REQUIRED)

add_subdirectory("src/a3-motion-engine")
add_subdirectory("src/a3-motion-ui")

set(ENABLE_TESTING TRUE CACHE BOOL "enable unit testing with GoogleTest")
if(ENABLE_TESTING)
add_subdirectory("src/a3-motion-tests")
endif()
