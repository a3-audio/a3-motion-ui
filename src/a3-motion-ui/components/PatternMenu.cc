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

#include "PatternMenu.hh"

#include <algorithm>

#include <a3-motion-engine/tempo/TempoClock.hh>

namespace a3
{

PatternMenu::PatternMenu (TempoClock const &tempoClock,
                          juce::Value &valueEncoderIncrement)
    : _tempoClock (tempoClock)
{
  valueEncoderIncrement.addListener (this);

  auto font = _labelLength.getFont ();
  font.setHeight (LayoutHints::fontSize);

  addChildComponent (_labelLength);
  _labelLength.setJustificationType (juce::Justification::centredLeft);
  // labelLength.setBorderSize ({ 0, 0, 0, 0 });
  _labelLength.setFont (font);
  _labelLength.setVisible (true);
  _labelLength.setText ("Bars", juce::dontSendNotification);

  addChildComponent (_labelLengthValue);
  _labelLengthValue.setJustificationType (juce::Justification::centredRight);
  // labelLengthValue.setBorderSize ({ 0, 0, 0, 0 });
  _labelLengthValue.setFont (font);
  _labelLengthValue.setVisible (true);

  updateLengthValue ();
}

void
PatternMenu::resized ()
{
  auto bounds = getLocalBounds ();
  auto boundsLength = bounds.withHeight (LayoutHints::lineHeight);
  _labelLength.setBounds (boundsLength);
  _labelLengthValue.setBounds (boundsLength);
}

void
PatternMenu::valueChanged (juce::Value &value)
{
  auto const increment = int (value.getValue ());
  jassert (increment == 1 || increment == -1);

  _lengthBarLog2 += increment;
  _lengthBarLog2
      = std::clamp (_lengthBarLog2, lengthBarMinLog2, lengthBarMaxLog2);

  updateLengthValue ();
}

juce::Value &
PatternMenu::getLengthBeats ()
{
  return _lengthBeats;
}

void
PatternMenu::updateLengthValue ()
{
  auto lengthBars = std::exp2 (_lengthBarLog2);
  _lengthBeats = static_cast<int> (lengthBars * _tempoClock.getBeatsPerBar ());

  if (_lengthBarLog2 >= 0)
    {
      _labelLengthValue.setText (juce::String (int (lengthBars)),
                                 juce::dontSendNotification);
    }
  else
    {
      _labelLengthValue.setText ("1/" + juce::String (int (1.f / lengthBars)),
                                 juce::dontSendNotification);
    }
}

}
