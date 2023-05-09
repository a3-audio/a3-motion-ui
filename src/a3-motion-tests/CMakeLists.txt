include(GoogleTest)

juce_add_console_app(a3-motion-tests
    COMPANY_NAME "a3-audio"
    PRODUCT_NAME "a3-motion-tests")
juce_generate_juce_header("a3-motion-tests")

target_sources("a3-motion-tests" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/TestRunnerApp.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/TempoClock.cc"
    )

target_link_libraries(a3-motion-tests PUBLIC
    a3-motion-engine
    gtest
)

gtest_discover_tests(a3-motion-tests)
