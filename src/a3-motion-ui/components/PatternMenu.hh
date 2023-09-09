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

#include <a3-motion-ui/components/LayoutHints.hh>

namespace a3
{

class TempoClock;

class PatternMenu : public juce::Component, public juce::Value::Listener
{
public:
  PatternMenu (TempoClock const &tempoClock,
               juce::Value &valueEncoderIncrement);
  void resized () override;

  void valueChanged (juce::Value &value) override;
  juce::Value &getLengthBeats ();

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight;
  }

private:
  void updateLengthValue ();

  TempoClock const &_tempoClock;

  int _lengthBarLog2 = 0;
  juce::Value _lengthBeats;
  juce::Label _labelLength;
  juce::Label _labelLengthValue;

  static constexpr auto lengthBarMinLog2 = -2;
  static constexpr auto lengthBarMaxLog2 = 4;
};

}
