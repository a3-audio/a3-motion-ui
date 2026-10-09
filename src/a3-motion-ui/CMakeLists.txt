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
    components/ChannelStrip.cc
    components/ChannelStrip.hh
    components/BrowserComponent.cc
    components/BrowserComponent.hh
    components/LineMapRenderer.cc
    components/MotionComponent.cc
    components/MotionComponent.hh
    components/SphereShader.cc
    components/SphereShader.hh
    components/LoopLengthDisplay.cc
    components/LoopLengthDisplay.hh
    components/PadRowDisplay.cc
    components/PadRowDisplay.hh
    components/ElevationDisplay.cc
    components/ElevationDisplay.hh
    components/DirectivityComponent.cc
    components/DirectivityComponent.hh
    components/MixerControls.hh
    tests/TempoEstimatorTest.cc
    tests/TempoEstimatorTest.hh
)

set(HARDWARE_INTERFACE_ENABLED FALSE CACHE BOOL "Enable interfacing with hardware")
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


# A switch over an enum that forgets a value is an error here, not a warning.
# On 2026-09-27 the pad handler lost its Play|Pause case in a rewrite; the
# compiler said so, and the line drowned in the float-equal warnings. Private,
# so it guards this target's own sources and nothing that includes them.
target_compile_options("a3-motion-ui" PRIVATE -Werror=switch)

target_compile_definitions("a3-motion-ui" PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    DONT_SET_USING_JUCE_NAMESPACE=1
    $<$<BOOL:${HARDWARE_INTERFACE_ENABLED}>:HARDWARE_INTERFACE_ENABLED>
    $<$<BOOL:${HARDWARE_INTERFACE_V2}>:HARDWARE_INTERFACE_V2>
    $<$<BOOL:${HARDWARE_INTERFACE_V3}>:HARDWARE_INTERFACE_V3>
    # Motion is a pure OSC interface; A3 Core does the audio (decided
    # 2026-10-06). With both off the binary holds no audio device type at all,
    # so nothing in it can open one. MotionIsPureOsc holds these two.
    JUCE_ALSA=0
    JUCE_JACK=0
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
    # Only because JUCE's Standalone plugin format refuses to compile without
    # these two (juce_audio_plugin_client_Standalone.cpp, #error). Motion opens
    # no audio device: see JUCE_ALSA/JUCE_JACK above.
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

# The UI sources the test runner exercises, compiled once for both
# (2026-10-03). Each target used to compile them itself, 93 files twice over.
# The library takes the app's flags rather than linking JUCE modules of its
# own: a JUCE module linked here would be compiled here once more, which costs
# more than the copies it saves.
add_library(a3-motion-ui-shared STATIC)
set_target_properties(a3-motion-ui-shared PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_sources(a3-motion-ui-shared PRIVATE
    FrameRate.cc
    FrameRate.hh
    WindowReport.cc
    WindowReport.hh
    io/SerialReconnect.cc
    io/SerialReconnect.hh
    io/SerialCandidates.cc
    io/SerialCandidates.hh
    osc/OscMessageHandler.cc
    osc/OscMessageHandler.hh
    osc/VuRouting.cc
    osc/VuRouting.hh
    components/LookAndFeel.cc
    components/LookAndFeel.hh
    components/StatusBar.cc
    components/StatusBar.hh
    components/StatusBarLayout.cc
    components/StatusBarLayout.hh
    components/fpv/BodyLook.cc
    components/fpv/BodyLook.hh
    components/fpv/FloorBodies.cc
    components/fpv/FloorBodies.hh
    components/fpv/FloorGesture.cc
    components/fpv/FloorGesture.hh
    components/fpv/FpvFloor.cc
    components/fpv/FpvFloor.hh
    components/fpv/FpvLayout.cc
    components/fpv/FpvLayout.hh
    components/fpv/FpvPagePress.hh
    components/fpv/FpvStrips.cc
    components/fpv/FpvStrips.hh
    components/fpv/ShipShape.cc
    components/fpv/ShipShape.hh
    components/AppView.hh
    components/BlobPunch.cc
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
    components/BrowserLayout.cc
    components/ControllerLayout.cc
    components/ListScroll.cc
    components/ListScroll.hh
    components/BrowserLayout.hh
    components/ControllerLayout.hh
    theme/FunctionKeyColours.cc
    theme/TransportLook.cc
    theme/PadStatusColours.cc
    components/ChannelValueReset.cc
    components/RecordingIndicator.cc
    components/ClockModeTempo.cc
    components/LibraryKeys.cc
    components/ActionEditing.cc
    components/ActionEditing.hh
    components/ScriptPanelLayout.cc
    components/ScriptPanelLayout.hh
    components/ScriptPanel.cc
    components/ScriptPanel.hh
    components/LibraryList.cc
    theme/SkinGroups.cc
    theme/SkinGroups.hh
    theme/SkinSections.cc
    theme/SkinSections.hh
    theme/CleanSkin.cc
    theme/CleanSkin.hh
    theme/FunctionKeyColours.hh
    theme/TransportLook.hh
    components/ClipSettingsLayout.hh
    components/CameraFingers.cc
    components/SourceKeys.cc
    components/SourceKeys.hh
    components/CameraFingers.hh
    components/TouchGrabs.cc
    components/TouchGrabs.hh
    components/SpeakerLightScaling.cc
    components/EnergyMap.cc
    components/SphereProjection.cc
    components/LineMapGeometry.cc
    components/LineMapStrokes.cc
    components/LineMapCapsules.cc
    components/ElevationSideView.cc
    components/Listener.cc
    io/ButtonLedColours.cc
    io/LedLoad.cc
    io/LedCache.cc
    io/OnScreenKeyboard.cc
    io/OnScreenKeyboard.hh
    io/PanelGrid.cc
    io/PanelGrid.hh
    io/PanelButtonCells.cc
    io/PanelButtonCells.hh
    io/PanelOwnership.cc
    io/PanelOwnership.hh
    components/PanelKeyboard.cc
    components/PanelKeyboard.hh
    io/Workspaces.cc
    io/Workspaces.hh
    components/WorkspaceList.cc
    components/WorkspaceList.hh
    theme/Theme.cc
    theme/ThemeColours.cc
    theme/ThemedComponent.cc
    theme/SkinParameters.cc
    theme/TextInput.cc
    components/TickIndicator.cc
    components/TickIndicator.hh
    components/TrajectoryIcon.cc
    components/TrajectoryIcon.hh
    components/GlobalSettingsComponent.cc
    components/SkinEditorComponent.cc
    components/SkinEditorComponent.hh
    components/SkinPanelLayout.cc
    components/SkinPanelLayout.hh
    components/SkinPanelComponent.cc
    components/SkinPanelComponent.hh
    components/GlobalSettingsComponent.hh
    components/ClipSettingsCaptions.cc
    components/PatternProgressBar.cc
    components/PatternProgressBar.hh
    components/ActionComponent.cc
    components/ActionLayout.cc
    components/ActionMotionKnobs.cc
    components/ActionChain.cc
    components/PendingScriptWrites.cc
    components/BlobPush.cc
    components/BarKnob.cc
    components/KnobHold.cc
    components/EncoderMap.cc
    components/BarKeyboardLayout.cc
    components/BarKeyboardLayout.hh
    components/BarKeyboardModel.cc
    components/BarKeyboardModel.hh
    components/BarKeyboardComponent.cc
    components/BarKeyboardComponent.hh
    components/PatternDisplay.cc
    components/ColumnBreak.cc
    components/ColumnBreak.hh
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
    components/ClipSettingsComponent.cc
    components/ClipSettingsComponent.hh
    components/ControllerComponent.cc
    components/ControllerComponent.hh
    components/ColourPickerComponent.cc
    components/ColourPickerComponent.hh
    components/FittedFont.cc
    components/FittedFont.hh
    ConfigFileWatcher.cc
    ConfigFileWatcher.hh
    SettingsPersistence.cc
    SessionFile.cc
    PatternDir.cc
    AppPaths.cc
    PendingTakes.cc
    TakeUnderway.cc
    SessionFile.hh
    PatternDir.hh
    AppPaths.hh
    PendingTakes.hh
    TakeUnderway.hh
    SettingsPersistence.hh
)

target_include_directories(a3-motion-ui-shared PRIVATE
    $<TARGET_PROPERTY:a3-motion-ui,INCLUDE_DIRECTORIES>)
target_compile_definitions(a3-motion-ui-shared PRIVATE
    $<TARGET_PROPERTY:a3-motion-ui,COMPILE_DEFINITIONS>)
target_compile_options(a3-motion-ui-shared PRIVATE
    $<TARGET_PROPERTY:a3-motion-ui,COMPILE_OPTIONS>)
target_link_libraries(a3-motion-ui-shared INTERFACE
    a3-motion-engine)

# The app's JuceHeader.h, generated here because this library compiles first:
# the app links it and so waits for it. Two owners of the one custom command
# would let make run it twice at once.
get_target_property(ui_library_code "a3-motion-ui" JUCE_GENERATED_SOURCES_DIRECTORY)
set(ui_defs_file $<GENEX_EVAL:$<TARGET_PROPERTY:a3-motion-ui,JUCE_DEFS_FILE>>)
add_custom_command(OUTPUT "${ui_library_code}/JuceHeader.h"
    COMMAND juce::juceaide header "${ui_defs_file}" "${ui_library_code}/JuceHeader.h"
    DEPENDS "${ui_defs_file}"
    VERBATIM)
target_sources(a3-motion-ui-shared PRIVATE "${ui_library_code}/JuceHeader.h")

target_link_libraries("a3-motion-ui" PUBLIC a3-motion-ui-shared)

a3_precompile_juce_header(a3-motion-ui-shared "a3-motion-ui")
a3_precompile_juce_header("a3-motion-ui")
