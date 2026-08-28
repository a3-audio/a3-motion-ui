include(GoogleTest)

juce_add_console_app(a3-motion-tests
    COMPANY_NAME "a3-audio"
    PRODUCT_NAME "a3-motion-tests")
juce_generate_juce_header("a3-motion-tests")

target_sources("a3-motion-tests" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/TestRunnerApp.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TempoClock.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Position.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/HeightMapSphere.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/MotionEngine.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SettingsPersistence.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OscMessageHandler.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/CoronaScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SpeakerLightScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ConfigFileWatcher.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/EnergyMap.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SphereProjection.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ButtonLedColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/Theme.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ThemeColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/LookAndFeelTheme.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/NoColourLiterals.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/FontScale.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ClipSettingsCaptions.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/ThemedComponent.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinSelection.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinParameters.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinFiles.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TextInput.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/SkinEditorTyping.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/OnScreenKeyboard.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TouchGrabs.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingSpans.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/PatternWrittenTicks.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/RecordingSeam.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/SettingsPersistence.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/osc/OscMessageHandler.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/CoronaScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/SpeakerLightScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/ConfigFileWatcher.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/EnergyMap.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/SphereProjection.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/io/ButtonLedColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/theme/Theme.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/theme/ThemeColours.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/LookAndFeel.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/ClipSettingsCaptions.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/theme/ThemedComponent.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/theme/SkinParameters.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/theme/TextInput.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/SkinEditorComponent.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/io/OnScreenKeyboard.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/TouchGrabs.cc"
    )

# The corona visibility test reads the config that actually ships with the app,
# so a tuning change that hides the effect fails the suite rather than the eye.
target_compile_definitions(a3-motion-tests PRIVATE
    A3_CONFIG_JSON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../../config/config.json"
    A3_UI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui"
    A3_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../resources")

target_include_directories("a3-motion-tests" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(a3-motion-tests PUBLIC
    a3-motion-engine
    # Colour work is most of the theme, and juce::Colour lives here. Without
    # it the LookAndFeel and every colour conversion would be untestable.
    juce::juce_graphics
    # LookAndFeel_V4 and the ColourIds it defines live here.
    juce::juce_gui_basics
    gtest
)

gtest_discover_tests(a3-motion-tests)
