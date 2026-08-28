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

#include <a3-motion-engine/Pattern.hh>

#include <optional>

namespace a3
{

/** What to do with the stretch across a take's loop point.
 *
 *  Only that one. A finger lifted mid-take lifted on purpose — that is a jump
 *  somebody played, and it is always held and then jumped. The seam is where
 *  the take happens to have started and stopped, which nobody played. */
enum class SeamMode
{
  /** Interpolate across the seam, so the end meets the start again. */
  Glide,
  /** Hold the last thing played, then jump at the loop point. */
  Hard,
};

/** Fill every stretch `pattern` never wrote.
 *
 *  A stretch in the middle is held at the position before it and then jumps —
 *  always, whatever `mode` says, because a finger lifted there lifted on
 *  purpose. Only the stretch across the loop point follows `mode`.
 *
 *  Does nothing to a pattern that wrote nothing at all — the caller discards
 *  such a take rather than filling it with guesses. */
/** `stopTick` is where the take stopped: the last tick the freshest pass
 *  wrote. Recording in Loop runs several passes, so the ticks after it still
 *  carry an earlier one, and the edge between them is where the motion visibly
 *  breaks. Nothing is missing there, so filling holes never reached it. Left
 *  out, the loop point is used instead. */
void closeRecordingSeams (Pattern &pattern, SeamMode mode,
                          std::optional<index_t> stopTick = {});

/** Fill the take's seam again, the other way.
 *
 *  How a seam is filled is a playback setting, not a property of the take —
 *  baking it in at the end of a recording would mean it could never be changed
 *  again. Each call starts from the two played positions at either end of the
 *  seam, never from the last fill, so switching back and forth does not drift.
 *
 *  Does nothing to a pattern whose seam has no length. */
void applySeamMode (Pattern &pattern, SeamMode mode);

}
