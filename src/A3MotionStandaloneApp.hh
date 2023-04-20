#pragma once

#include "A3MotionStandaloneApp.hh"

#include <JuceHeader.h>

#include "A3MotionMainWindow.hh"

class A3MotionStandaloneApp
    : public JUCEApplication
{
public:
    A3MotionStandaloneApp()  {}
    ~A3MotionStandaloneApp() {}

    void initialise(String const & commandLine) override;
    void shutdown() override;

    const String getApplicationName() override;
    const String getApplicationVersion() override;

    void systemRequestedQuit() override;

private:
    std::unique_ptr<A3MotionMainWindow> mainWindow;
};

// this generates boilerplate code to launch our app class:
START_JUCE_APPLICATION (A3MotionStandaloneApp)
