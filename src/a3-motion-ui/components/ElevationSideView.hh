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

#include <a3-motion-ui/components/SphereProjection.hh>

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
  /** Where the point sits in the circle, -1 to 1 each way from its middle.
   *
   *  A projection, not a ruler. The circle is a second view of the room, kept
   *  a quarter turn from the one above it: overhead up there is a side view
   *  down here, and a side view up there is an overhead down here -- so the
   *  pair of them always shows the room from two directions at once, and what
   *  one loses the other has. */
  float across;
  float down;

  /** On the far side of the room, i.e. behind the middle. Drawn dimmer, the
   *  way the sphere dims what is behind it -- without that, a figure that
   *  goes round the listener reads as a flat squiggle. */
  bool behind;

  /** Nothing joins this point to the one before it: the pen lifts here.
   *
   *  It is decided in the room and not in the picture, because the picture
   *  hides the one case it matters for. Where the mapping tears at the pad's
   *  centre the sound jumps clear across the room, but near the ceiling the
   *  circle is narrow, so both ends of that jump land a few pixels apart and a
   *  pen watching only the drawn distance keeps writing -- which is a straight
   *  line through the elevation circle that no sound ever made. */
  bool startsStroke;
};

/** Where the circle is looking from, given where the sphere above is.
 *
 *  A quarter turn behind it, always. With the sphere overhead this is the side
 *  view the circle has always been; lean the sphere to the horizon and this
 *  comes up to overhead, so the picture that loses the height is never the
 *  only one you have. */
SphereCamera elevationSideCamera (SphereCamera sphere);

/** Put a direction on the sphere into the circle, as seen from there. */
ElevationSidePoint elevationSideView (Pos const &direction,
                                      SphereCamera sphere = {});

/** And back: what direction a finger in the circle is pointing at.
 *
 *  The near side of it, which is the half the finger can see. `across` and
 *  `down` run -1 to 1 from the middle of the circle; outside it the nearest
 *  direction on the rim is given rather than nothing, so a finger that slides
 *  off the edge keeps setting a value instead of stopping dead. */
Pos elevationSideDirection (float across, float down,
                            SphereCamera sphere = {});

/** The whole figure, sampled down to at most `maxPoints` -- the circle is a
 *  couple of centimetres across, and a thousand ticks in it is ink, not
 *  information. Invalid ticks are dropped rather than drawn at the origin. */
std::vector<ElevationSidePoint>
elevationSideView (std::vector<Pos> const &directions, std::size_t maxPoints,
                   SphereCamera sphere = {});

}
