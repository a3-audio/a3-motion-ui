cmake_minimum_required(VERSION 3.11)

set(PROJECT "a3-motion-ui")
project(${PROJECT} VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)

find_package(JUCE CONFIG REQUIRED)

add_subdirectory("src/a3-motion-engine")
add_subdirectory("src/a3-motion-ui")

# # unit testing
# set(ENABLE_TESTING TRUE CACHE BOOL "enable unit testing with GoogleTest")
# if(ENABLE_TESTING)
#     include(GoogleTest)

#     add_executable(
#         a3-motion-ui-tests
#         "${CMAKE_CURRENT_SOURCE_DIR}/tests/TestRunnerApplication.cc"
#         "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/TempoClock.cc"
#     )
#     target_include_directories(
#         a3-motion-ui-tests PRIVATE
#         "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT}_artefacts/JuceLibraryCode"
#     )
#     target_link_libraries(
#         a3-motion-ui-tests
#         a3-motion-ui
#         gtest
#     )

#     gtest_discover_tests(a3-motion-ui-tests)
# endif()
