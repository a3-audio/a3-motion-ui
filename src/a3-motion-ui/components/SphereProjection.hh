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

#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** The projection the sphere display actually uses: an orthographic view of
 *  the upper hemisphere from above. The centre of the disc is straight up, the
 *  rim is the horizon.
 *
 *  This is what the drawing does — a blob is drawn by dropping z — so it is
 *  also what a drag has to invert. The height map's 2D space is a different
 *  thing: it parametrises a pattern's own coordinate range through its `reach`,
 *  where radius 1 sits 45 degrees off the zenith rather than on the horizon.
 *  Reading a finger's radius as a pattern radius is what left the blob short of
 *  the finger.
 *
 *  Takes and returns HOA cartesian positions; the screen conversion stays with
 *  `cartesian2DHOA2JUCE` in Helpers.hh. */

/** Direction on the unit sphere a point of the disc stands for. Only the x/y
 *  of `flat` are read. Beyond the rim there is no direction left above the
 *  horizon, so it is held there rather than returning a NaN. */
Pos discToDirection (Pos const &flat);

/** Where a direction lands on the disc — dropping z, which is what
 *  orthographic projection comes to. */
Pos directionToDisc (Pos const &direction);

}
