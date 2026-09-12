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

namespace a3
{

/** How long a full sweep of a channel value takes at most, in milliseconds.
 *
 *  A value that arrives at A3 Core is passed straight through to a REAPER
 *  parameter -- there is no interpolation at the far end -- so every message
 *  is a step, and a step big enough is a click in the room. Nothing here can
 *  make the far end smooth; what it can do is never hand it a big step.
 *
 *  Thirty milliseconds rather than the usual anti-zipper five: the ticks this
 *  runs on are two to eight milliseconds apart depending on tempo, so five
 *  would be one or two messages and no better than the jump it replaced.
 *  Thirty is four to a dozen, and is still under what a hand reads as a delay.
 *
 *  It limits a *rate*, so it is invisible to anything already moving slower
 *  than this -- an accent with the shortest attack the envelope has still
 *  takes an eighth of a second, and passes through untouched. */
constexpr double potSlewMillis = 30.;

/** The next value on a ramp towards @p target that never moves faster than a
 *  full 0..1 sweep in @p rampMillis, given how long since the last step.
 *
 *  Lands on the target *exactly* rather than approaching it: the whole
 *  contract of a channel value at rest is that it is what the hand set, to the
 *  bit, and a smoothing that only ever got close would leave every knob a
 *  little off wherever it stopped. That is also why it is a linear ramp rather
 *  than the one-pole filter this would usually be -- a one-pole never arrives.
 *
 *  A ramp time of zero, or an elapsed time that has not advanced, is not an
 *  error: the first is smoothing switched off and the second is two calls
 *  inside one clock resolution. Both give the target and the current value
 *  back respectively, so a caller never has to special-case them. */
float slewTowards (float current, float target, double elapsedMillis,
                   double rampMillis);

}
