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

#pragma once

#include "StandaloneApp.hh"

#include <JuceHeader.h>

#include "MainWindow.hh"

namespace a3
{

class StandaloneApp : public juce::JUCEApplication
{
public:
  StandaloneApp () {}
  ~StandaloneApp ();

  void initialise (juce::String const &commandLine) override;
  void shutdown () override;

  const juce::String getApplicationName () override;
  const juce::String getApplicationVersion () override;

  void systemRequestedQuit () override;

private:
  void setupFileLogger ();

  std::unique_ptr<MainWindow> mainWindow;
  std::unique_ptr<juce::SplashScreen> splash;
  std::unique_ptr<juce::Logger> logger;
};

} // namespace a3 end

// this generates boilerplate code to launch our app class:
START_JUCE_APPLICATION (a3::StandaloneApp)