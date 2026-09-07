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

#include <juce_core/juce_core.h>

#include <a3-motion-engine/util/Types.hh>

#include <optional>
#include <vector>

namespace a3
{

/** A gap that is drawn through, the point it leads to, and how long the blob
 *  is given to get there.
 *
 *  `viaTick` is `fromTick + 1` unless the bias sends it elsewhere.
 *
 *  A crossing used to be taken in the single tick the gap sits in, however far
 *  it reached -- which is the blob shooting across the room: on a take of a
 *  few thousand ticks an ordinary step is a thousandth or two, and half a
 *  sphere in one tick is hundreds of times that. So the crossing is given
 *  time, and the time is taken out of the two ends it joins, because a take
 *  has a fixed length and a fixed bar and there is nowhere else for it to come
 *  from. The blob leaves the timeline at `leaveTick` and is back on it
 *  `windowTicks` later.
 *
 *  What is paid for it is geometry at the ends: the tail of the run before and
 *  the head of the run after are not visited. On a tapped take -- which is
 *  what bridges are for -- that costs nothing, because those ends are standing
 *  still, and standing still a little less is not something anyone can hear.
 */
struct Bridge
{
  index_t fromTick;
  index_t viaTick;
  /** Where the blob leaves the timeline. `fromTick` when nothing is
   *  reserved, which is what a fade of nothing comes to. */
  index_t leaveTick;
  /** How many ticks the crossing takes, counted from `leaveTick`. At least
   *  one, which is the old behaviour exactly. */
  index_t windowTicks;
};

/** Which of a take's gaps are drawn through, and where each one leads.
 *
 *  Playback and drawing both read this rather than each deciding for itself
 *  what counts as a gap. Two independent answers drift apart, and then the
 *  sphere shows a line the blob does not run on.
 */
struct BridgePlan
{
  std::vector<Bridge> bridges; ///< sorted by fromTick

  bool bridged (index_t tick) const;
  std::optional<index_t> via (index_t tick) const;

  /** The crossing this tick falls inside, if any -- the blob is off the
   *  timeline for the whole of a crossing, not only on the gap's own tick.
   *  `numTicks` is the take's length, because a crossing may wrap past the
   *  end of it. */
  Bridge const *crossingAt (index_t tick, index_t numTicks) const;

  /** Ticks the blob never stands on, because a crossing is under way and it
   *  is somewhere between the two ends instead. The drawn line asks the same
   *  question, so that it gives up exactly the ends the movement gives up --
   *  two independent answers drift apart, and then the sphere shows a line
   *  the blob does not run on.
   *
   *  The three ticks a crossing does stand on are kept: where it leaves,
   *  where it lands, and where it rejoins. */
  bool skipsTick (index_t tick, index_t numTicks) const;
};

/** The seed a take's bridges are drawn from.
 *
 *  Taken from the tick data rather than stored or built from the name: no new
 *  field in the file format, and renaming a clip does not change where it
 *  goes. Two clips on the same shape share it, which is right -- the same
 *  movement has the same holes.
 */
juce::int64 seedForTicks (std::vector<Pos> const &ticks);

/** Work out which gaps to draw through.
 *
 *  @param fadeReach   0..1, the distance a bridge may span, where 1 is the
 *                     sphere's full diameter and closes every gap.
 *                     It also says how much of the two ends a crossing may
 *                     reserve: at nothing, none, and the crossing is the
 *                     single-tick jump it always was; wide open, half of each
 *                     of the runs it joins.
 *  @param bridgeBias  -4..+4. Zero leads to the next tick in time; negative
 *                     to the spatially nearest reachable tick, positive to a
 *                     random one. The magnitude mixes rather than switches.
 */
BridgePlan planBridges (std::vector<Pos> const &ticks, float fadeReach,
                        int bridgeBias, juce::int64 seed);

}
