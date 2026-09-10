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

#include <juce_gui_basics/juce_gui_basics.h>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

namespace a3
{

/** The one button face — a wash and a thin edge, never a filled slab, so a
 *  button reads as part of the bar rather than pasted on it.
 *
 *  A free function beside paintBarKnob, and for the same
 *  reason: the mixer overlay and the bar are on screen at the same moment,
 *  the overlay covering the sphere and the bar staying visible beneath it, so
 *  two button faces are two faces a hand sees side by side. The mixer's PFL,
 *  FX and filter MODE keys had grown a private copy of this — the same two
 *  rounded rectangles with the same three alphas written out again — and a
 *  brighter skin would have retuned one of them. The word on the face went
 *  the same way and took longer to notice: it is `Colours::barText` now, the
 *  one that ClipSettingsComponent and BarKnob also read.
 *
 *  Only an active button carries colour, and that is the state talking rather
 *  than the button. `isSelected` says the colour is the channel's: a button
 *  that belongs to no channel — the global section's four — lights grey.
 *
 *  @param label   The value line, and the whole of a button that names itself.
 *  @param caption The name over it. Empty for a button whose label is its name.
 *  @param valueColour A value with a colour of its own, the clock's mode being
 *                     the one there is. Transparent leaves it the bar's.
 */
void paintBarButton (juce::Graphics &g, juce::Rectangle<int> bounds,
                     ControlMetrics metrics, juce::Colour channelColour,
                     juce::String const &label, juce::String const &caption,
                     bool isActive, bool isSelected,
                     juce::Colour valueColour = {});

}
