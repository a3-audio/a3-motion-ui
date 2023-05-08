cmake_minimum_required(VERSION 3.11)

set(PROJECT "a3-motion-ui")
project(${PROJECT} VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)

find_package(JUCE CONFIG REQUIRED)

add_subdirectory("src/a3-motion-engine")
add_subdirectory("src/a3-motion-ui")

# unit testing
set(ENABLE_TESTING TRUE CACHE BOOL "enable unit testing with GoogleTest")
if(ENABLE_TESTING)
    include(GoogleTest)

    add_executable(
        a3-motion-tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRunnerApplication.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/TempoClock.cc"
    )

    _juce_initialise_target("a3-motion-tests")
    juce_generate_juce_header("a3-motion-tests")

    target_link_libraries(a3-motion-tests PUBLIC
        a3-motion-engine
        gtest
        gtest_main
    )

    gtest_discover_tests(a3-motion-tests)
endif()
