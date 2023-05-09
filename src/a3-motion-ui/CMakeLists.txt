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
    FORMATS "Standalone;VST3"                   # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
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
    LookAndFeel.cc
    LookAndFeel.hh
    LayoutHints.cc
    LayoutHints.hh
    components/MainComponent.cc
    components/MainComponent.hh
    components/ChannelHeader.cc
    components/ChannelHeader.hh
    components/ChannelFooter.cc
    components/ChannelFooter.hh
    components/MotionComponent.cc
    components/MotionComponent.hh
)

target_compile_definitions("a3-motion-ui" PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    DONT_SET_USING_JUCE_NAMESPACE=1
)

target_include_directories("a3-motion-ui" PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries("a3-motion-ui" PUBLIC
    a3-motion-engine
    juce::juce_audio_utils
    juce::juce_gui_basics
)
