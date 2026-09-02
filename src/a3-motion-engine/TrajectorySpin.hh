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

#include <JuceHeader.h>

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** Spinning a trajectory: the whole recorded shape turns around the vertical
 *  axis while the blob keeps running along it.
 *
 *  It is one rotation of the *recorded 2D* position around the origin, applied
 *  at playback time and never written into the take. The 2D disc's radius is
 *  the elevation and its angle is the azimuth (see HeightMap::mapTo3D()), so
 *  turning the disc turns the trajectory around the pole and leaves every
 *  point at the height it was recorded at.
 *
 *  The speed is held as a signed power of two rather than a rate, because a
 *  rotation that does not land on a bar line is a rotation that argues with
 *  the loop under it. Sign is direction: positive turns the trajectory the
 *  way a clock does, *as drawn on the screen* — see spinPosition(), which is
 *  where that is arranged and why it is not simply a positive rotation.
 */

/** The steps either side of a standstill. Eight is where the table runs out
 *  of musically useful lengths: a quarter bar per revolution is already four
 *  turns to the bar. */
constexpr int spinMaxStep = 8;

/** How many bars one revolution takes, at |step| 1..spinMaxStep: 32, 16, 8,
 *  4, 2, 1, 1/2, 1/4. Zero for a standstill, which has no length. */
float spinBarsPerRevolution (int step);

/** Revolutions per bar, signed — the rate the phase actually advances at. */
float spinRevolutionsPerBar (int step);

/** One tick on. `phase` and the result are in revolutions, wrapped to [0, 1):
 *  a rotation has no history, so nothing is gained by letting it grow and
 *  everything is lost once it is large enough that adding a tick to it does
 *  nothing. */
float advanceSpinPhase (float phase, int step, float ticksPerBar);

/** Turn a recorded 2D position by `phase` revolutions around the origin. */
Pos spinPosition (Pos const &position, float phase);

}
