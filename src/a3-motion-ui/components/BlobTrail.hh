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

#include <cmath>

namespace a3
{

/** The wake hung off a blob: four points, each chasing the one in front of it.
 *
 *  A chain rather than a remembered history of positions, and the difference
 *  is what the effect is for. A history traces the take exactly -- which the
 *  neon line under the blob already does, so a second one says nothing. A
 *  chain cuts the corners of a tight figure and settles onto the blob when it
 *  stops, which is what something heavy being dragged through the room looks
 *  like.
 *
 *  Positions are the sphere's normalised units, the same ones the shader is
 *  handed. Nothing here draws: MotionComponent advances one of these per
 *  channel per frame and passes the four points on as uniforms. */
struct BlobTrail
{
  /** Eight rather than four, because four was measurably short: at the lag
   *  the device ships with, a plume over four links is spent before the eye
   *  has followed it. More links rather than a slower chase -- a slower chase
   *  lengthens the *delay* and leaves the same four marks strung further
   *  apart, which reads as a dotted line rather than as a trail. */
  static constexpr int numLinks = 8;

  float x[numLinks] = {};
  float y[numLinks] = {};

  /** Whether the chain has ever been given a position. An unprimed chain is
   *  laid onto the blob whole instead of being dragged out of the origin. */
  bool primed = false;
};

/** Nothing to trail -- the channel has no valid position this frame.
 *
 *  Kept apart from advancing it so that a channel stopping here and starting
 *  again over there is not given a wake joining the two places. */
inline void
releaseBlobTrail (BlobTrail &trail)
{
  trail.primed = false;
}

/** One frame on.
 *
 *  `lag` is how hard each link chases the one in front, per frame; 0 is a
 *  chain that never moves and 1 one that sits on the blob. `cutDistance` is
 *  what counts as a jump rather than a movement -- a clip looping back to its
 *  start, or a finger dropping the blob somewhere else -- and past it the
 *  chain is laid down afresh rather than dragged across a path nothing
 *  travelled. */
inline void
advanceBlobTrail (BlobTrail &trail, float x, float y, float lag,
                  float cutDistance)
{
  auto const jumped
      = std::hypot (x - trail.x[0], y - trail.y[0]) > cutDistance;

  if (!trail.primed || jumped)
    {
      for (int k = 0; k < BlobTrail::numLinks; ++k)
        {
          trail.x[k] = x;
          trail.y[k] = y;
        }
      trail.primed = true;
      return;
    }

  auto leadX = x;
  auto leadY = y;
  for (int k = 0; k < BlobTrail::numLinks; ++k)
    {
      trail.x[k] += (leadX - trail.x[k]) * lag;
      trail.y[k] += (leadY - trail.y[k]) * lag;
      leadX = trail.x[k];
      leadY = trail.y[k];
    }
}

}
