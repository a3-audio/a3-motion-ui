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

#include "PotKnob.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

PotKnob::PotKnob ()
{
  // Up and down, like every other value on this device: the encoders turn,
  // the screen is dragged.
  setSliderStyle (juce::Slider::RotaryVerticalDrag);
  setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
  setRange (0.0, 1.0);
  setVelocityBasedMode (false);
  setDoubleClickReturnValue (false, 0.0);

  setColour (juce::Slider::thumbColourId, toColour (theme ().textPrimary));
  refreshSensitivity ();
}

void
PotKnob::refreshSensitivity ()
{
  // In the knob's own heights rather than in pixels: the skin's pot size then
  // decides the feel along with the look, and neither follows a screen.
  setMouseDragSensitivity (juce::jmax (
      1, juce::roundToInt (static_cast<float> (getHeight ())
                           * knobHeightsForTheWholeRange)));
}

void
PotKnob::mouseDoubleClick (juce::MouseEvent const &event)
{
  if (onDoubleTapped)
    {
      onDoubleTapped ();
      return;
    }

  juce::Slider::mouseDoubleClick (event);
}

void
PotKnob::resized ()
{
  juce::Slider::resized ();
  refreshSensitivity ();
}

void
PotKnob::setLabel (juce::String const &label)
{
  if (_label == label)
    return;

  _label = label;
  repaint ();
}

void
PotKnob::setFillsFromTheMiddle (bool fills)
{
  if (_fillsFromTheMiddle == fills)
    return;

  _fillsFromTheMiddle = fills;
  repaint ();
}

void
PotKnob::setWraps (bool wraps)
{
  if (_wraps == wraps)
    return;

  _wraps = wraps;
  repaint ();
}

void
PotKnob::setReach (float reach)
{
  if (juce::approximatelyEqual (_reach, reach))
    return;

  _reach = reach;
  repaint ();
}

void
PotKnob::setKnobColour (juce::Colour colour)
{
  if (findColour (juce::Slider::thumbColourId) == colour)
    return;

  setColour (juce::Slider::thumbColourId, colour);
  repaint ();
}

void
PotKnob::setActive (bool active)
{
  if (_active == active)
    return;

  _active = active;
  repaint ();
}

void
PotKnob::setSelected (bool selected)
{
  if (_selected == selected)
    return;

  _selected = selected;
  repaint ();
}

}
