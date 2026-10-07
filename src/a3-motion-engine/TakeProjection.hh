/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#pragma once

#include <a3-motion-engine/Playhead.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class Pattern;

/** A take's 2D tick and the direction it is heard at, both ways (#66).
 *
 *  Playback does four things to a stored point, in this order: squeeze and
 *  turn it in the plane (rotate plus the spin's phase, the squeezes swept by
 *  their stretch), project it through the band (swept by swell and sway),
 *  and lean the result in the room (tilt and roll, swept by their own
 *  sweeps). playedPosition() is that chain and nothing else; the engine plays
 *  every tick through it.
 *
 *  writtenPosition() is the same chain backwards, so a take recorded over a
 *  clip that is turned, squeezed or leant plays back where the finger was --
 *  decided 2026-10-07: a take inherits the clip's settings, and recording
 *  undoes them. Every step is exact except the band, which holds a direction
 *  it cannot play at the nearest one it can (HeightMapSphere::mapTo2D); the
 *  round trip played(written(d)) is therefore the direction itself inside the
 *  band and its nearest playable neighbour outside.
 *
 *  Both read the knobs and the sweeps' phases off the pattern as they stand,
 *  so a tick in the future is reached by first laying that tick's phases onto
 *  the take: setPassPhases(). */
Pos playedPosition (HeightMap const &heightMap, Pos const &position2D,
                    Pattern const &pattern);

Pos writtenPosition (HeightMap const &heightMap, Pos const &direction,
                     Pattern const &pattern);

/** How many clock ticks into its first pass a clip plays its tick `tick`,
 *  of `numTicks`, in a pass `passTicks` clock ticks long.
 *
 *  Forward and Bounce set off from the first tick, Reverse from the last;
 *  Bounce spreads its outward leg over one tick fewer, the way
 *  fractionalTickForPlayback() reads it. Random enters at a phase drawn as
 *  it starts, which nothing knows at recording time, so it is answered as
 *  Forward -- the same figure entered somewhere else. Only the first pass
 *  can be answered at all: a sweep whose cycle is not a whole number of
 *  passes is somewhere else on the next one, which is what a sweep is for. */
float ticksIntoFirstPass (index_t tick, index_t numTicks,
                          PlayDirection direction, float passTicks);

/** Lay onto `pattern` the phase every sweep -- spin, both stretches, swell,
 *  sway, tilt and roll sweeps -- has `ticks` clock ticks into a pass begun by
 *  MotionEngine::beginPass(), at the steps it has now. Zero ticks is the
 *  pass's start: every phase back to zero. */
void setPassPhases (Pattern &pattern, float ticks, float ticksPerBar);

}
