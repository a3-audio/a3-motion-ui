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
        "CCACHE_NOHASHDIR=true"
        # What ccache needs to cache a compile that reads a precompiled header.
        "CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime"
        "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_LAUNCHER})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_LAUNCHER})
endif()

# A precompiled JuceHeader.h per target (2026-10-03): every source parsed all
# of JUCE's headers anew, about four of its five seconds. JUCE's own module
# units are left out: they set JUCE's configuration before they include it,
# and a header put in front of them would come first.
get_target_property(juce_core_sources juce::juce_core INTERFACE_SOURCES)
list(GET juce_core_sources 0 juce_core_unit)
get_filename_component(juce_modules_dir "${juce_core_unit}" DIRECTORY)
get_filename_component(juce_modules_dir "${juce_modules_dir}" DIRECTORY)
file(GLOB juce_module_units
    "${juce_modules_dir}/*/*.cpp" "${juce_modules_dir}/*/*.c" "${juce_modules_dir}/*/*.mm")

# header_owner: the target whose generated JuceHeader.h is used; the target
# itself unless given.
function(a3_precompile_juce_header target)
    set(header_owner ${target})
    if(ARGC GREATER 1)
        set(header_owner ${ARGV1})
    endif()
    get_target_property(juce_library_code ${header_owner} JUCE_GENERATED_SOURCES_DIRECTORY)
    set_source_files_properties(${juce_module_units} TARGET_DIRECTORY ${target}
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    target_precompile_headers(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${juce_library_code}/JuceHeader.h>")
endfunction()

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
