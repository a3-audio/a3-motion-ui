#pragma once

#include "StandaloneApp.hh"

#include <JuceHeader.h>

#include "MainWindow.hh"

namespace a3
{

class StandaloneApp : public JUCEApplication
{
public:
  StandaloneApp () {}
  ~StandaloneApp () {}

  void initialise (String const &commandLine) override;
  void shutdown () override;

  const String getApplicationName () override;
  const String getApplicationVersion () override;

  void systemRequestedQuit () override;

private:
  std::unique_ptr<MainWindow> mainWindow;
  std::unique_ptr<SplashScreen> splash;
};

} // namespace a3 end

// this generates boilerplate code to launch our app class:
START_JUCE_APPLICATION (a3::StandaloneApp)
