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

/** The bar's knob: an arc, a pointer and a caption under it.
 *
 *  A free function rather than a method, because the ACTION page draws the
 *  same knob and must not draw a second, slightly different one. The channel's
 *  colour is passed in for the same reason -- it was the only thing tying this
 *  to the bar it grew up in.
 *
 *  @param angleFrac    -1..1 across the scale.
 *  @param fillFromZero Bipolar: the arc grows from the middle rather than from
 *                      the start, so which side of standstill you are on reads
 *                      at a glance.
 *  @param reachFrac    Where something else is holding the value right now --
 *                      the spin under the rotation. -2 for nothing.
 *  @param wraps        A closed ring, for a value that comes round to itself.
 */
void paintBarKnob (juce::Graphics &g, juce::Rectangle<int> bounds,
                   ControlMetrics metrics, juce::Colour channelColour,
                   juce::String const &label, float angleFrac,
                   bool fillFromZero, bool isActive, bool isSelected,
                   float reachFrac = -2.f, bool wraps = false);

}
