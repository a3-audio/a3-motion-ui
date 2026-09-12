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
 *  How fast it turns is a TempoLfo step — bars per revolution, off the tempo
 *  clock. Positive turns the trajectory the way a clock does *as drawn on the
 *  screen*; see spinPosition(), which is where that is arranged and why it is
 *  not simply a positive rotation.
 */

/** Turn a recorded 2D position by `phase` revolutions around the origin. */
Pos spinPosition (Pos const &position, float phase);

}
