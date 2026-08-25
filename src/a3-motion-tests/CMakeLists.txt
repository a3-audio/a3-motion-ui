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
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/SettingsPersistence.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/osc/OscMessageHandler.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/CoronaScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/SpeakerLightScaling.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/ConfigFileWatcher.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/EnergyMap.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/components/SphereProjection.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../a3-motion-ui/io/ButtonLedColours.cc"
    )

# The corona visibility test reads the config that actually ships with the app,
# so a tuning change that hides the effect fails the suite rather than the eye.
target_compile_definitions(a3-motion-tests PRIVATE
    A3_CONFIG_JSON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../../config/config.json"
    A3_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../resources")

target_link_libraries(a3-motion-tests PUBLIC
    a3-motion-engine
    gtest
)

gtest_discover_tests(a3-motion-tests)
