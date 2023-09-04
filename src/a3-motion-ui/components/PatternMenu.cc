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

namespace a3
{

PatternMenu::PatternMenu (juce::Value &valueEncoderIncrement)
{
  valueEncoderIncrement.addListener (this);

  auto font = labelLength.getFont ();
  font.setHeight (LayoutHints::fontSize);

  addChildComponent (labelLength);
  labelLength.setJustificationType (juce::Justification::centredLeft);
  // labelLength.setBorderSize ({ 0, 0, 0, 0 });
  labelLength.setFont (font);
  labelLength.setVisible (true);
  labelLength.setText ("Bars", juce::dontSendNotification);

  addChildComponent (labelLengthValue);
  labelLengthValue.setJustificationType (juce::Justification::centredRight);
  // labelLengthValue.setBorderSize ({ 0, 0, 0, 0 });
  labelLengthValue.setFont (font);
  labelLengthValue.setVisible (true);
  updateLengthValue ();
}

void
PatternMenu::resized ()
{
  auto bounds = getLocalBounds ();
  auto boundsLength = bounds.withHeight (LayoutHints::lineHeight);
  labelLength.setBounds (boundsLength);
  labelLengthValue.setBounds (boundsLength);
}

void
PatternMenu::valueChanged (juce::Value &value)
{
  auto const increment = int (value.getValue ());
  jassert (increment == 1 || increment == -1);

  lengthLog2 += increment;
  lengthLog2 = std::clamp (lengthLog2, lengthMinLog2, lengthMaxLog2);

  updateLengthValue ();
}

void
PatternMenu::updateLengthValue ()
{
  if (lengthLog2 >= 0)
    {
      labelLengthValue.setText (juce::String (std::exp2 (lengthLog2)),
                                juce::dontSendNotification);
    }
  else
    {
      labelLengthValue.setText ("1/" + juce::String (std::exp2 (-lengthLog2)),
                                juce::dontSendNotification);
    }
}

}
