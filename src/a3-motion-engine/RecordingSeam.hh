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

/** What to do with the stretches a recording never wrote. */
enum class SeamMode
{
  /** Glide across: interpolate between the written position before the
   *  stretch and the one after it. Across the loop point this is what closes
   *  end to start, without the seam needing a case of its own. */
  Glide,
  /** Hold the position before the stretch, then jump. For a deliberate hard
   *  change at the loop point. */
  Hard,
};

/** Fill every stretch `pattern` never wrote.
 *
 *  There is no difference between a hole in the middle and the seam between
 *  end and start: both are an unwritten stretch between two written ones, and
 *  the seam is the one that runs across the loop point. One rule serves both.
 *
 *  Does nothing to a pattern that wrote nothing at all — the caller discards
 *  such a take rather than filling it with guesses. */
void closeRecordingSeams (Pattern &pattern, SeamMode mode);

}
