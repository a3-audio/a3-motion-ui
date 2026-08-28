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
void closeRecordingSeams (Pattern &pattern, SeamMode mode);

}
