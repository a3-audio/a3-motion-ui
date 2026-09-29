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

#include <juce_graphics/juce_graphics.h>

#include <vector>

namespace a3
{

/** How far a blob is drawn off its channel's place while a held blob pushes
 *  it aside, in the sphere's 2D pixel space (#56, 2026-09-29).
 *
 *  Only the drawing: the channel stays where it is in the room. A blob
 *  within `radius` of a held one is pushed out onto that circle; one that
 *  is clear slides back towards its own place along the circle's edge, so it
 *  slips round the held blob rather than through it.
 *
 *  @param blob    where the channel is, in pixels
 *  @param offset  how far it is drawn off that now
 *  @param held    every held blob, in pixels
 *  @return        the offset for this frame */
juce::Point<float> nextPushOffset (juce::Point<float> blob,
                                   juce::Point<float> offset,
                                   std::vector<juce::Point<float> > const &held,
                                   float radius);

/** Nothing held any more: the offset one frame closer to nothing, and
 *  exactly nothing once it is below a pixel. */
juce::Point<float> easedPushOffset (juce::Point<float> offset);

}
