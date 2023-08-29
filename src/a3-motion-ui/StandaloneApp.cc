/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "StandaloneApp.hh"

#include <a3-motion-engine/UserConfig.hh>

namespace a3
{

StandaloneApp::~StandaloneApp () {}

void
StandaloneApp::initialise (juce::String const &commandLine)
{
  juce::ignoreUnused (commandLine);

  setupFileLogger ();

  auto appNameVer = getApplicationName () + " " + getApplicationVersion ();
  juce::Logger::writeToLog (appNameVer);

  if (juce::JSON::parse (juce::File::getCurrentWorkingDirectory ()
                             .getChildFile ("config/config.json")
                             .loadFileAsString (),
                         userConfig)
          .failed ())
    {
      throw std::runtime_error ("could not parse config/config.json");
    }

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

  _mainWindow = std::make_unique<MainWindow> (getApplicationName ());
  // mainWindow->setBounds (0, 0, 600, 1024);
  _mainWindow->setBounds (0, 0, 450, 768);
  _mainWindow->setVisible (true);
}

void
StandaloneApp::setupFileLogger ()
{
  auto fileExecutable = juce::File::getSpecialLocation (
      juce::File::SpecialLocationType::currentExecutableFile);
  auto filenameLog = fileExecutable.getFileNameWithoutExtension () + ".log";

  _logger = std::make_unique<juce::FileLogger> (
      fileExecutable.getParentDirectory ().getChildFile (filenameLog),
      "a3-motion-ui debug log", 0);
  juce::Logger::setCurrentLogger (_logger.get ());
}

void
StandaloneApp::shutdown ()
{
  userConfig = juce::var{};

  // explicit deletion of MainWindow to capture tear-down messages
  // with our logger
  _mainWindow = nullptr;
  juce::Logger::setCurrentLogger (nullptr);
}

juce::String const
StandaloneApp::getApplicationName ()
{
  return "A3 Motion UI";
}

juce::String const
StandaloneApp::getApplicationVersion ()
{
  return "0.0.0";
}

void
StandaloneApp::systemRequestedQuit ()
{
  juce::Logger::writeToLog ("systemRequestedQuit()");
}

}
