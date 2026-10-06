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

namespace
{
SourceKey
keyOf (juce::MouseInputSource const &source)
{
  auto const type = source.isTouch () ? SourceKey::touch
                    : source.isPen () ? SourceKey::pen
                                      : SourceKey::mouse;
  return { type, source.getIndex () };
}

bool
isStillDown (SourceKey key)
{
  for (auto const &source : juce::Desktop::getInstance ().getMouseSources ())
    if (keyOf (source) == key)
      return source.isDragging ();
  return false;
}
}

void
PotKnob::mouseDown (juce::MouseEvent const &event)
{
  // One finger, two sources on the device: the touch and X's emulated mouse
  // (#64). Only the first moves the knob; the second would re-anchor the
  // drag and pull the value along a stream of its own.
  _gesture.forgetIfNotDown (isStillDown);
  if (!_gesture.press (keyOf (event.source)))
    return;

  juce::Slider::mouseDown (event);
}

void
PotKnob::mouseDrag (juce::MouseEvent const &event)
{
  if (_gesture.follows (keyOf (event.source)))
    juce::Slider::mouseDrag (event);
}

void
PotKnob::mouseUp (juce::MouseEvent const &event)
{
  if (_gesture.release (keyOf (event.source)))
    juce::Slider::mouseUp (event);
}

void
PotKnob::mouseDoubleClick (juce::MouseEvent const &event)
{
  // The same finger's second source double-taps too; one reset is enough.
  if (!_gesture.press (keyOf (event.source)))
    return;

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
PotKnob::setWriting (bool writing)
{
  if (writing == _writing)
    return;

  _writing = writing;
  repaint ();
}

void
PotKnob::setLaneDriven (bool driven)
{
  if (driven == _laneDriven)
    return;

  _laneDriven = driven;
  repaint ();
}

void
PotKnob::setEncoderMarked (bool marked)
{
  if (marked == _encoderMarked)
    return;

  _encoderMarked = marked;
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
