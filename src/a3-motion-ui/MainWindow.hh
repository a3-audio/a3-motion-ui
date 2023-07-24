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

#include <JuceHeader.h>

#include <a3-motion-ui/MotionController.hh>

namespace a3
{

class MainWindow : public juce::ResizableWindow
{
public:
  MainWindow (juce::String const &name);

  void userTriedToCloseWindow () override;
  void resized () override;

private:
  bool keyPressed (const juce::KeyPress &k) override;

  juce::Viewport _viewport;
  MotionController _motionController;
};

}
