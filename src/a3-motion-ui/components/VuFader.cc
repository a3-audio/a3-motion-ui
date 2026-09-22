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

#include "VuFader.hh"

#include <a3-motion-ui/components/VuMeter.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

VuFader::VuFader ()
{
  setSliderStyle (juce::Slider::LinearVertical);
  setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
  setRange (0.0, 1.0);

  // Relative: the value moves with the finger from where it stood, so a
  // finger landing low on a loud channel does not pull it down.
  setSliderSnapsToMousePosition (false);

  // No acceleration: a fader travels with the hand. Velocity mode turns a
  // quick move into a big one, which is the opposite of a fader.
  setVelocityBasedMode (false);

  // JUCE's own double click puts a slider back to a default. Ours says
  // "full", and the master's says nothing -- see onDoubleTapped.
  setDoubleClickReturnValue (false, 0.0);

  setColour (juce::Slider::thumbColourId, toColour (theme ().textPrimary));
}

void
VuFader::resized ()
{
  // First, or the slider never lays its own track out: its region stays one
  // pixel, and the handle is drawn nowhere at all.
  juce::Slider::resized ();

  auto const bounds = getLocalBounds ();
  auto const travel
      = bounds.getHeight () - vuFaderHandle (bounds, 0.f).getHeight ();

  setMouseDragSensitivity (juce::jmax (1, travel));
}

void
VuFader::setHandleColour (juce::Colour colour)
{
  if (findColour (juce::Slider::thumbColourId) == colour)
    return;

  setColour (juce::Slider::thumbColourId, colour);
  repaint ();
}

void
VuFader::mouseDoubleClick (juce::MouseEvent const &event)
{
  if (onDoubleTapped)
    {
      onDoubleTapped ();
      return;
    }

  juce::Slider::mouseDoubleClick (event);
}

}
