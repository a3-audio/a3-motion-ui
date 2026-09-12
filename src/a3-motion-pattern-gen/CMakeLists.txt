add_executable(a3-pattern-gen main.cc)

target_link_libraries(a3-pattern-gen PRIVATE a3-motion-engine)

# Need the engine's generated JuceHeader.h on the include path
target_include_directories(a3-pattern-gen PRIVATE
    "${CMAKE_BINARY_DIR}/src/a3-motion-engine/a3-motion-engine_artefacts/JuceLibraryCode")

target_compile_definitions(a3-pattern-gen PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    DONT_SET_USING_JUCE_NAMESPACE=1)

set_target_properties(a3-pattern-gen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
