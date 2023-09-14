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

PatternMenu::PatternMenu (TempoClock const &tempoClock)
    : _tempoClock (tempoClock)
{
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
PatternMenu::increaseLength ()
{
  updateLengthValue ();
}

void
PatternMenu::decreaseLength ()
{
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

void
PatternMenu::setIsRecording (bool isRecording)
{
  _isRecording = isRecording;

  if (isRecording)
    {
      _labelLength.setColour (juce::Label::textColourId, juce::Colours::red);
      _labelLengthValue.setColour (juce::Label::textColourId,
                                   juce::Colours::red);
    }
  else
    {
      _labelLength.setColour (juce::Label::textColourId, juce::Colours::white);
      _labelLengthValue.setColour (juce::Label::textColourId,
                                   juce::Colours::white);
    }
}

}
