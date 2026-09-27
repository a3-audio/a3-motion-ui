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

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** The blob on its way over a jump of the playhead.
 *
 *  DIRECTION = Rnd drops in at a random point of the figure at every pass
 *  end. Followed literally the blob teleports across the room; since
 *  2026-09-27 it glides there instead, from where it was to where the
 *  playhead now runs, arriving on the line when the glide's ticks are spent.
 *  Along the sphere rather than through it, so it does not pass under the
 *  listener's nose. */
struct Glide
{
  Pos from;
  index_t ticksLeft = 0;
  index_t total = 0;
};

/** How many ticks a glide takes: a beat at least, never more than half a
 *  pass, and GAP-CONNECTOR's fade lengthens it up to that half -- the time a
 *  bridge may reserve wide open. */
index_t glideTicks (float fadeReach, index_t passTicks, index_t beatTicks);

/** A glide leaving from `from`, taking `ticks` ticks; none for zero. */
Glide startGlide (Pos from, index_t ticks);

/** Where the blob is this tick while `target` is where the playhead puts it:
 *  one tick of the glide spent, eased in and out. `target` itself once the
 *  glide is over, or when there is none. */
Pos glidedPosition (Glide &glide, Pos target);

}
