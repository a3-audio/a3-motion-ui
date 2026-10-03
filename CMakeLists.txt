cmake_minimum_required(VERSION 3.11)

set(PROJECT "a3-motion-ui")
project(${PROJECT} VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig)
find_package(JUCE CONFIG REQUIRED)

# ccache, when it is installed (2026-10-03): a fresh worktree rebuilt all of
# JUCE and every source -- some twenty minutes on the rig. The base directory
# makes the paths of different worktrees compare equal, so one worktree's
# compile is another's cache hit.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    # The checkout is the base: seen from its build/ every source path is the
    # same in every worktree, and JUCE, outside it, stays absolute and equal.
    set(CCACHE_LAUNCHER "${CMAKE_COMMAND}" -E env "CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}"
        "CCACHE_NOHASHDIR=true" "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_LAUNCHER})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_LAUNCHER})
endif()

add_subdirectory("src/a3-motion-engine")
add_subdirectory("src/a3-audio-engine")
add_subdirectory("src/a3-motion-ui")
add_subdirectory("src/a3-motion-pattern-gen")
add_subdirectory("src/a3-motion-clip-gen")

set(TESTS_ENABLED TRUE CACHE BOOL "enable unit testing with GoogleTest")
if(TESTS_ENABLED)
# Without this CMake writes no CTestTestfile.cmake, and `ctest` in the build
# directory reports "No tests were found" however many gtest_discover_tests
# calls there are below. The suite was only ever reachable by running the
# test binary itself.
enable_testing()
add_subdirectory("src/a3-motion-tests")
endif()
