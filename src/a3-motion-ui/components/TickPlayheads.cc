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

#include "TickPlayheads.hh"

namespace a3
{

juce::Rectangle<float>
playheadBounds (juce::Rectangle<float> tick, float fraction, float width)
{
  if (fraction < 0.f)
    return {};

  // The mark travels across what is left of the indicator once its own width
  // is accounted for, so the last position sits flush with the right edge
  // rather than two pixels past it. A playhead is drawn over the indicator,
  // not inside it, and one hanging off the end would paint on the status bar.
  auto const travel = juce::jmax (0.f, tick.getWidth () - width);

  return { tick.getX () + travel * juce::jlimit (0.f, 1.f, fraction),
           tick.getY (), width, tick.getHeight () };
}

}
