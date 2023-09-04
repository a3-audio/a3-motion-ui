juce_add_plugin("a3-motion-ui"
    # VERSION ...                               # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    COMPANY_NAME "a3-audio"                     # Specify the name of the plugin's author
    IS_SYNTH FALSE                              # Is this a synth or an effect?
    NEEDS_MIDI_INPUT FALSE                      # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE                     # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                        # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE            # Does the editor need keyboard focus?
    COPY_PLUGIN_AFTER_BUILD FALSE               # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE A3AU               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE A3MC                            # A unique four-character plugin id with at least one upper-case character
    PLUGIN_NAME "A3 Motion UI"
    FORMATS "Standalone"                        # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME "a3-motion-ui"                 # The name of the final executable, which can differ from the target name
)

juce_generate_juce_header("a3-motion-ui")

target_sources("a3-motion-ui" PRIVATE
    A3MotionAudioProcessor.cc
    A3MotionAudioProcessor.hh
    A3MotionEditor.cc
    A3MotionEditor.hh
    StandaloneApp.cc
    StandaloneApp.hh
    MainWindow.cc
    MainWindow.hh
    io/InputOutputAdapter.cc
    io/InputOutputAdapter.hh
    components/A3MotionUIComponent.cc
    components/A3MotionUIComponent.hh
    components/LookAndFeel.cc
    components/LookAndFeel.hh
    components/LayoutHints.cc
    components/LayoutHints.hh
    components/StatusBar.cc
    components/StatusBar.hh
    components/ChannelHeader.cc
    components/ChannelHeader.hh
    components/ChannelStrip.cc
    components/ChannelStrip.hh
    components/MotionComponent.cc
    components/MotionComponent.hh
    components/Ticks.cc
    components/Ticks.hh
    components/PatternMenu.cc
    components/PatternMenu.hh
    tests/TempoEstimatorTest.cc
    tests/TempoEstimatorTest.hh
)

set(HARDWARE_INTERFACE_ENABLED FALSE CACHE BOOL "Enable interfacing with hardware")
if(${HARDWARE_INTERFACE_ENABLED})
    set(HARDWARE_INTERFACE_VERSION "V2" CACHE STRING "Hardware interface to use, options are: V2")
else()
    unset(HARDWARE_INTERFACE_VERSION CACHE)
endif()

set(HARDWARE_INTERFACE_LIBRARIES "")
set(HARDWARE_INTERFACE_V2 FALSE)
if("${HARDWARE_INTERFACE_VERSION}" STREQUAL "V2")
    set(HARDWARE_INTERFACE_V2 TRUE)
    target_sources("a3-motion-ui" PRIVATE
        io/InputOutputAdapterV2.cc
        io/InputOutputAdapterV2.hh
    )
    set(HARDWARE_INTERFACE_LIBRARIES "serial;gpiod")
endif()


target_compile_definitions("a3-motion-ui" PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    DONT_SET_USING_JUCE_NAMESPACE=1
)

target_include_directories("a3-motion-ui" PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/extern"
)

target_link_libraries("a3-motion-ui" PUBLIC
    a3-motion-engine
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_gui_basics
    juce::juce_opengl
)

if(${HARDWARE_INTERFACE_ENABLED})
target_link_libraries("a3-motion-ui" PRIVATE
    ${HARDWARE_INTERFACE_LIBRARIES}
)

endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Config.hh.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/Config.hh"
)
