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

namespace a3
{

/** The length a pattern falls back to when nothing else says — one bar at 4/4.
 *  Every pattern that ships carries its own `data-beats`, so this is for a slot
 *  that holds nothing yet. */
constexpr float defaultPatternLengthBeats = 4.f;

/** How long one traversal takes, in beats.
 *
 *  The pattern's own length taken at the clip's rate. Speed used to *be* the
 *  length — turning it therefore redefined how long a take had been, after the
 *  fact. It is a factor now, and the direction is unchanged: a larger exponent
 *  is a longer traversal, which is slower. Zero plays the pattern at the length
 *  it was recorded at. */
float playbackLengthBeats (float patternLengthBeats, int speedLog2);

/** The same length counted in ticks.
 *
 *  Beats are the wrong unit to hand a traversal on in: a whole number of them
 *  cannot say "half a beat", and clamped up to one it cannot say anything
 *  faster either. On a four-beat pattern that ceiling sits at 2^-2, so the four
 *  detents of the speed knob below it changed the reading and nothing else.
 *
 *  Never zero: a traversal of no length would divide by nothing. */
index_t playbackLengthTicks (float lengthBeats, index_t ticksPerBeat);

}
