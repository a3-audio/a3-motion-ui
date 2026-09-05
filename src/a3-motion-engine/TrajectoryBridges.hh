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

/** A gap that is drawn through, and the point it leads to.
 *
 *  `viaTick` is `fromTick + 1` unless the bias sends it elsewhere. */
struct Bridge
{
  index_t fromTick;
  index_t viaTick;
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
 *  @param bridgeBias  -4..+4. Zero leads to the next tick in time; negative
 *                     to the spatially nearest reachable tick, positive to a
 *                     random one. The magnitude mixes rather than switches.
 */
BridgePlan planBridges (std::vector<Pos> const &ticks, float fadeReach,
                        int bridgeBias, juce::int64 seed);

}
