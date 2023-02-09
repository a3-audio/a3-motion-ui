#include "A3MotionStandaloneApp.hh"

void
A3MotionStandaloneApp::
initialise (String const & commandLine)
{
    Logger::writeToLog(
        getApplicationName() + " " + getApplicationVersion());

    mainWindow = std::make_unique<A3MotionMainWindow>(getApplicationName());
    mainWindow->setVisible(true);
}

void
A3MotionStandaloneApp::
shutdown()
{
}

String const
A3MotionStandaloneApp::
getApplicationName()
{
    return "A3 Motion UI";
}

String const
A3MotionStandaloneApp::
getApplicationVersion()
{
    return "0.0.0";
}
