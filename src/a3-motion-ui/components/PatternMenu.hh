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

class PatternMenu : public juce::Component, public juce::Value::Listener
{
public:
  PatternMenu (juce::Value &valueEncoderIncrement);
  void resized () override;

  void valueChanged (juce::Value &value) override;

  static constexpr int
  getMinimumHeight ()
  {
    return LayoutHints::lineHeight;
  }

  int getLengthBeats (int beatsPerBar) const;

private:
  void updateLengthValue ();

  juce::Label labelLength;
  juce::Label labelLengthValue;
  int lengthLog2 = 0;

  static constexpr auto lengthMinLog2 = -2;
  static constexpr auto lengthMaxLog2 = 4;
};

}
