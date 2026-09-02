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

namespace a3
{

/** The clock the clip's slow movements run on.
 *
 *  Everything that moves on its own here — the trajectory turning under the
 *  blob, the coverage opening and closing — moves at the same kind of speed,
 *  held the same way: a **signed power of two, in bars per cycle**, counted
 *  off the tempo clock rather than the wall clock.
 *
 *  That is not decoration. A movement whose cycle is a whole number of bars
 *  comes back to where it started on a bar line, so it agrees with the loop
 *  underneath it instead of drifting through it, and it follows a tempo
 *  change instead of being left behind by one. A free rate would be finer to
 *  set and wrong on every fourth bar.
 *
 *  The sign is a direction, and what that means is the caller's to say: for
 *  the spin it is which way round the sphere, for a sweep it is which way out
 *  of the value you set.
 */

/** The steps either side of a standstill. Eight is where the table runs out of
 *  musically useful lengths: a quarter bar per cycle is already four of them
 *  to the bar. */
constexpr int lfoMaxStep = 8;

/** How many bars one cycle takes, at |step| 1..lfoMaxStep: 32, 16, 8, 4, 2, 1,
 *  1/2, 1/4. Zero for a standstill, which has no length. */
float lfoBarsPerCycle (int step);

/** Cycles per bar, signed — the rate the phase actually advances at. */
float lfoCyclesPerBar (int step);

/** One tick on. `phase` and the result are in cycles, wrapped to [0, 1): a
 *  cycle has no history, so nothing is gained by letting the number grow and
 *  everything is lost once it is large enough that adding a tick does
 *  nothing. */
float advanceLfoPhase (float phase, int step, float ticksPerBar);

/** How far through its travel the cycle is: 0 at the start, 1 at the halfway
 *  point, 0 again at the end, on a raised cosine.
 *
 *  Not a triangle, because the turn at each end is where the eye looks: a
 *  corner there reads as the movement being yanked round, and the whole point
 *  of these is that they are slow enough to watch. */
float lfoTravel (float phase);

/** A 0..1 parameter swept out of where it was set and back again: `from` at
 *  phase 0, all the way to the end `step`'s sign points at by the halfway
 *  point, back to `from` at the end.
 *
 *  Which end rather than how far, because how far then answers itself — you
 *  say where the value sits and the sweep says which way it breathes, and
 *  there is no setting at which the two conspire to do nothing. */
float lfoSweep (float from, int step, float phase);

}
