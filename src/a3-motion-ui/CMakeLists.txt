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
    FrameRate.cc
    FrameRate.hh
    WindowReport.cc
    WindowReport.hh
    MainWindow.hh
    io/SerialReconnect.cc
    io/SerialReconnect.hh
    io/InputOutputAdapter.cc
    io/InputOutputAdapter.hh
    osc/OscMessageHandler.cc
    osc/OscMessageHandler.hh
    components/A3MotionUIComponent.cc
    components/A3MotionUIComponent.hh
    components/LookAndFeel.cc
    components/LookAndFeel.hh
    components/StatusBar.cc
    components/StatusBar.hh
    components/StatusBarLayout.cc
    components/StatusBarLayout.hh
    components/ChannelStrip.cc
    components/ChannelStrip.hh
    components/CoronaScaling.cc
    components/DragAccumulator.cc
    components/DragAccumulator.hh
    components/PotKnob.cc
    components/ScriptEditor.cc
    components/ScriptEditor.hh
    components/PotKnob.hh
    components/VuFader.cc
    components/VuMeterView.cc
    components/VuMeterView.hh
    components/VuFader.hh
    components/TouchControl.cc
    components/TouchControl.hh
    components/OverlayButtons.cc
    components/OverlayButtons.hh
    components/OverlaySideStrips.cc
    components/OverlaySideStrips.hh
    components/BarButton.cc
    components/BarButton.hh
    components/ClipSettingsLayout.cc
    components/RecordingLength.cc
    components/RecordingLength.hh
    components/BrowserComponent.cc
    components/BrowserLayout.cc
    components/ControllerLayout.cc
    components/ListScroll.cc
    components/ListScroll.hh
    components/BrowserComponent.hh
    components/BrowserLayout.hh
    components/ControllerLayout.hh
    components/ControllerComponent.cc
    components/ControllerComponent.hh
    theme/FunctionKeyColours.cc
    theme/TransportLook.cc
    theme/PadStatusColours.cc
    components/ChannelValueReset.cc
    components/RecordingIndicator.cc
    components/LibraryKeys.cc
    components/LibraryList.cc
    theme/SkinGroups.cc
    theme/SkinGroups.hh
    theme/CleanSkin.cc
    theme/CleanSkin.hh
    theme/FunctionKeyColours.hh
    theme/TransportLook.hh
    components/ClipSettingsLayout.hh
    components/TouchGrabs.cc
    components/TouchGrabs.hh
    components/SpeakerLightScaling.cc
    components/EnergyMap.cc
    components/SphereProjection.cc
    components/LineMapGeometry.cc
    components/ElevationSideView.cc
    components/Listener.cc
    io/ButtonLedColours.cc
    io/LedCache.cc
    io/OnScreenKeyboard.cc
    io/OnScreenKeyboard.hh
    theme/Theme.cc
    theme/ThemeColours.cc
    theme/ThemedComponent.cc
    theme/SkinParameters.cc
    theme/TextInput.cc
    components/MotionComponent.cc
    components/MotionComponent.hh
    components/SphereShader.cc
    components/SphereShader.hh
    components/TickIndicator.cc
    components/TickPlayheads.cc
    components/TickPlayheads.hh
    components/TickIndicator.hh
    components/LoopLengthDisplay.cc
    components/LoopLengthDisplay.hh
    components/PadRowDisplay.cc
    components/PadRowDisplay.hh
    components/TrajectoryIcon.cc
    components/TrajectoryIcon.hh
    components/ElevationDisplay.cc
    components/ElevationDisplay.hh
    components/DirectivityComponent.cc
    components/DirectivityComponent.hh
    components/GlobalSettingsComponent.cc
    components/ColourPickerComponent.cc
    components/ColourPickerComponent.hh
    components/SkinEditorComponent.cc
    components/SkinEditorComponent.hh
    components/GlobalSettingsComponent.hh
    components/ClipSettingsCaptions.cc
    components/PatternProgressBar.cc
    components/PatternProgressBar.hh
    components/ActionComponent.cc
    components/ActionLayout.cc
    components/BarKnob.cc
    components/ColumnBreak.cc
    components/ColumnBreak.hh
    components/MixerControls.hh
    components/VuMeter.cc
    components/VuMeter.hh
    components/MixerState.cc
    components/MixerState.hh
    components/MixerLayout.cc
    components/MixerLayout.hh
    components/MixerComponent.cc
    components/MixerComponent.hh
    components/MixerStripComponent.cc
    components/MixerStripComponent.hh
    components/FittedFont.cc
    components/FittedFont.hh
    components/ClipSettingsComponent.cc
    components/ClipSettingsComponent.hh
    ConfigFileWatcher.cc
    ConfigFileWatcher.hh
    SettingsPersistence.cc
    SessionFile.cc
    PendingTakes.cc
    SessionFile.hh
    PendingTakes.hh
    SettingsPersistence.hh
    tests/TempoEstimatorTest.cc
    tests/TempoEstimatorTest.hh
)

set(HARDWARE_INTERFACE_ENABLED FALSE CACHE BOOL "Enable interfacing with hardware")
# Off for the device build: on the NUC, A3 Core does the audio and this app
# must not open an audio device of its own.
set(A3_AUDIO_ENGINE_ENABLED FALSE CACHE BOOL "Run the audio engine inside the app")
if(${HARDWARE_INTERFACE_ENABLED})
    set(HARDWARE_INTERFACE_VERSION "V3" CACHE STRING "Hardware interface to use, options are: V2, V3")
else()
    unset(HARDWARE_INTERFACE_VERSION CACHE)
endif()

set(HARDWARE_INTERFACE_LIBRARIES "")
set(HARDWARE_INTERFACE_V2 FALSE)
set(HARDWARE_INTERFACE_V3 FALSE)
if("${HARDWARE_INTERFACE_VERSION}" STREQUAL "V2")
    set(HARDWARE_INTERFACE_V2 TRUE)
    target_sources("a3-motion-ui" PRIVATE
        io/InputOutputAdapterV2.cc
        io/InputOutputAdapterV2.hh
    )
    set(HARDWARE_INTERFACE_LIBRARIES "serial;gpiod")
elseif("${HARDWARE_INTERFACE_VERSION}" STREQUAL "V3")
    set(HARDWARE_INTERFACE_V3 TRUE)
    target_sources("a3-motion-ui" PRIVATE
        io/InputOutputAdapterV3.cc
        io/InputOutputAdapterV3.hh
    )
    set(HARDWARE_INTERFACE_LIBRARIES "serial;gpiod")
endif()


target_compile_definitions("a3-motion-ui" PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    DONT_SET_USING_JUCE_NAMESPACE=1
    $<$<BOOL:${HARDWARE_INTERFACE_ENABLED}>:HARDWARE_INTERFACE_ENABLED>
    $<$<BOOL:${HARDWARE_INTERFACE_V2}>:HARDWARE_INTERFACE_V2>
    $<$<BOOL:${HARDWARE_INTERFACE_V3}>:HARDWARE_INTERFACE_V3>
    $<$<BOOL:${A3_AUDIO_ENGINE_ENABLED}>:A3_AUDIO_ENGINE_ENABLED>
    # JACK only with the engine: jackd holds the sound card on the dev machine,
    # so ALSA alone cannot reach it. JUCE dlopens libjack at runtime and needs
    # only its headers to build; the device build opens no audio at all.
    $<$<BOOL:${A3_AUDIO_ENGINE_ENABLED}>:JUCE_JACK=1>
)

target_include_directories("a3-motion-ui" PUBLIC
    # Before the source tree: see the note in a3-motion-engine/CMakeLists.txt.
    "${CMAKE_CURRENT_BINARY_DIR}/.."
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/extern"
)

target_link_libraries("a3-motion-ui" PUBLIC
    a3-motion-engine
    a3-audio-engine
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_gui_basics
    # ColourSelector lives here: the colour field and the hue strip on the
    # picker are its, rather than a pair we drew by hand.
    juce::juce_gui_extra
    juce::juce_opengl
)

if(${HARDWARE_INTERFACE_ENABLED})
target_link_libraries("a3-motion-ui" PRIVATE
    ${HARDWARE_INTERFACE_LIBRARIES}
)

endif()

# Into the build directory -- see the note in a3-motion-engine/CMakeLists.txt.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Config.hh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Config.hh"
)
