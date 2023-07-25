add_library(juce_dependencies OBJECT)

target_link_libraries(juce_dependencies
    PUBLIC
        juce::juce_osc
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        juce::juce_recommended_lto_flags)

target_compile_definitions(juce_dependencies
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        DONT_SET_USING_JUCE_NAMESPACE=1
    INTERFACE
    $<TARGET_PROPERTY:juce_dependencies,COMPILE_DEFINITIONS>
)

target_include_directories(juce_dependencies
    INTERFACE
        $<TARGET_PROPERTY:juce_dependencies,INCLUDE_DIRECTORIES>)

add_library("a3-motion-engine" STATIC)
set_target_properties("a3-motion-engine" PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

_juce_initialise_target("a3-motion-engine")
juce_generate_juce_header("a3-motion-engine")

target_sources("a3-motion-engine" PRIVATE
    Channel.cc
    Channel.hh
    Master.cc
    Master.hh
    MotionEngine.cc
    MotionEngine.hh
    TempoClock.cc
    TempoClock.hh
    AsyncCommandQueue.cc
    AsyncCommandQueue.hh
    backends/SpatBackend.hh
    backends/SpatBackendIEM.cc
    backends/SpatBackendIEM.hh
    backends/SpatBackendA3.cc
    backends/SpatBackendA3.hh
    elevation/HeightMap.hh
    elevation/HeightMapFlat.cc
    elevation/HeightMapFlat.hh
    elevation/HeightMapSphere.cc
    elevation/HeightMapSphere.hh
    util/Timing.cc
    util/Timing.hh
    util/Geometry.hh
    util/Helpers.hh
)

target_include_directories("a3-motion-engine" PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries("a3-motion-engine" PUBLIC
    juce_dependencies
)
