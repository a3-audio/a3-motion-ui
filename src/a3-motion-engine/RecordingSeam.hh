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
#include <vector>

namespace a3
{

/** Fill every stretch `pattern` never wrote.
 *
 *  Every stretch is held at the position before it and then jumps -- the
 *  take's own join included. A finger that lifted there lifted on purpose, and
 *  whether a hole is drawn through is the fade's business at playback: a
 *  reading of the movement rather than a change to it.
 *
 *  `stopTick` is where the take stopped: the last tick the freshest pass
 *  wrote. Recording in Loop runs several passes, so the ticks after it still
 *  carry an earlier one, and the edge between them is where the motion visibly
 *  breaks. Nothing is missing there, so filling holes never reached it. Left
 *  out, the loop point is used instead.
 *
 *  Does nothing to a pattern that wrote nothing at all -- the caller discards
 *  such a take rather than filling it with a guess.
 */
void closeRecordingSeams (Pattern &pattern,
                          std::optional<index_t> stopTick = std::nullopt);

}
