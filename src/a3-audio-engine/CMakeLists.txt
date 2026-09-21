add_library(audio_dependencies OBJECT)

target_link_libraries(audio_dependencies
    PUBLIC
        juce::juce_audio_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(audio_dependencies
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        DONT_SET_USING_JUCE_NAMESPACE=1
    INTERFACE
    $<TARGET_PROPERTY:audio_dependencies,COMPILE_DEFINITIONS>
)

target_include_directories(audio_dependencies
    INTERFACE
        $<TARGET_PROPERTY:audio_dependencies,INCLUDE_DIRECTORIES>)

add_library("a3-audio-engine" STATIC)

set_target_properties("a3-audio-engine" PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_sources("a3-audio-engine" PRIVATE
    ControlSurface.cc
    ControlSurface.hh
    SpatBackendInternal.cc
    SpatBackendInternal.hh
)

target_include_directories("a3-audio-engine" PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# SpatBackend.hh pulls in OscAddresses.hh, which needs JuceHeader.h. That is
# generated per-target by JUCE's own CMake helpers and kept PRIVATE to
# a3-motion-engine, so linking against it publicly is not enough -- reuse its
# already-generated header directory instead of generating a second copy.
target_include_directories("a3-audio-engine" PRIVATE
    $<TARGET_PROPERTY:a3-motion-engine,JUCE_GENERATED_SOURCES_DIRECTORY>
)

target_link_libraries("a3-audio-engine" PUBLIC
    a3-motion-engine
    audio_dependencies
)
