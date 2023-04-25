#include "StandaloneApp.hh"

namespace a3
{

void
StandaloneApp::initialise (String const &commandLine)
{
  auto appNameVer = getApplicationName () + " " + getApplicationVersion ();

  Logger::writeToLog (appNameVer);

  // splash = std::make_unique<SplashScreen> (
  //     appNameVer,
  //     ImageFileFormat::loadFrom (File ("./resources/a3_logo-dark.png")),
  //     true /* useDropShadow */);

  // auto waitDurationMs = 2000;
  // Time::waitForMillisecondCounter (Time::getMillisecondCounter ()
  //                                  + waitDurationMs);
  // splash = nullptr;
  // splash->deleteAfterDelay (RelativeTime::seconds (3),
  //                           true /* removeOnMouseClick */);

  mainWindow = std::make_unique<MainWindow> (getApplicationName ());
  mainWindow->setBounds (0, 0, 500, 800);
  mainWindow->setVisible (true);
}

void
StandaloneApp::shutdown ()
{
}

String const
StandaloneApp::getApplicationName ()
{
  return "A3 Motion UI";
}

String const
StandaloneApp::getApplicationVersion ()
{
  return "0.0.0";
}

void
StandaloneApp::systemRequestedQuit ()
{
  Logger::writeToLog ("systemRequestedQuit()");
}

}
