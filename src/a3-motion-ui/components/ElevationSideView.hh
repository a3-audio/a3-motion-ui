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

#include <vector>

#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** One point of a trajectory as the elevation circle draws it.
 *
 *  The circle in CLIP > Elevation is the room seen from the side, from where
 *  the performer stands: height up the picture, left and right across it, and
 *  the depth axis pointing away into the screen. It is the same room the
 *  sphere above shows from overhead, turned a quarter so that the one thing
 *  the sphere cannot say -- how high the sound is -- is the one thing this
 *  says plainly. */
struct ElevationSidePoint
{
  /** Colatitude as a fraction: 0 at the ceiling, 1 at the floor. This is the
   *  same number the base line and the clip cuts are drawn at, so a point and
   *  the line it belongs to always meet. */
  float frac;

  /** Where the point sits across the picture, -1 at the left edge of the
   *  circle and +1 at the right. It is a bearing, not a distance: the width
   *  available at this height is the circle's own chord there. */
  float across;

  /** On the far side of the room, i.e. behind the middle. Drawn dimmer, the
   *  way the sphere dims what is behind it -- without that, a figure that
   *  goes round the listener reads as a flat squiggle. */
  bool behind;
};

/** Put a direction on the sphere into the side-on picture.
 *
 *  The overhead view puts the room's +x up the screen and its +y to the left
 *  (cartesian2DHOA2JUCE). The side view keeps that horizontal axis and trades
 *  the vertical one for height, so the viewer stands at the bottom of the
 *  overhead picture and looks into it -- turn your head up from the sphere to
 *  this circle and the room has not moved. */
ElevationSidePoint elevationSideView (Pos const &direction);

/** The whole figure, sampled down to at most `maxPoints` -- the circle is a
 *  couple of centimetres across, and a thousand ticks in it is ink, not
 *  information. Invalid ticks are dropped rather than drawn at the origin. */
std::vector<ElevationSidePoint>
elevationSideView (std::vector<Pos> const &directions, std::size_t maxPoints);

}
