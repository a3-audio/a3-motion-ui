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

#include <a3-motion-engine/util/Types.hh>

#include <vector>

namespace a3
{

/** Where a trajectory teleports rather than travels.
 *
 *  Index `i` means the step from tick `i` to tick `i + 1` is a jump; the last
 *  index may be `size - 1`, the step that wraps back to the start, because a
 *  pattern is a loop and that step is as real as any other.
 *
 *  Decided against the trajectory's own typical step rather than against a
 *  fixed distance: what counts as a teleport in a pattern drawn across the
 *  whole sphere is an ordinary step in one tapped into a corner of it. */
std::vector<size_t> trajectoryJumps (std::vector<Pos> const &ticks);

/** What this trajectory calls an ordinary step: the median distance between
 *  neighbouring ticks, so the jumps being looked for cannot drag the yardstick
 *  they are measured against. Zero when there is nothing to measure. */
float typicalTrajectoryStep (std::vector<Pos> const &ticks);

/** Past this, a step is a teleport rather than a movement. */
float trajectoryJumpThreshold (float typicalStep);

/** Whether the pattern was tapped in rather than drawn — a handful of held
 *  positions with teleports between them.
 *
 *  This cannot be read off the invalid ticks. A take is only sparse while it
 *  is being played in; closing its seams fills every gap by holding the last
 *  position, so a finished tap take has no invalid ticks at all and looks,
 *  tick for tick, exactly like a drawn one. The difference is that almost
 *  none of its steps move. */
bool isTappedTrajectory (std::vector<Pos> const &ticks);

/** One position per held stretch, in tick order: the taps themselves, without
 *  the repeats that hold them. */
std::vector<Pos> trajectoryPlateaus (std::vector<Pos> const &ticks);

/** The runs actually travelled through, cut wherever the data is missing or
 *  teleports. Drawing them as one stroke is what put a chord across every
 *  jump. Cut linearly, not around the loop: the icon is a stroke, and the
 *  step that wraps is simply never drawn. */
std::vector<std::vector<Pos> >
trajectorySegments (std::vector<Pos> const &ticks);

}
