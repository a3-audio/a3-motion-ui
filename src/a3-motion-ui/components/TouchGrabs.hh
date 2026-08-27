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

#include <map>
#include <optional>
#include <vector>

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/**
 * Which finger is holding which channel.
 *
 * Until JUCE 9 there was a single grabbed channel, and that was enough: Linux
 * delivered every touch as the one X core pointer, so a second finger could
 * only ever look like the first one moving. JUCE 9 gives each touch its own
 * MouseInputSource with a stable index, and from there the questions are all
 * per source — which channel is this finger dragging, which channels are
 * already somebody else's, and which one channel does lifting this finger hand
 * back to playback.
 *
 * A source that is down but grabbed nothing is remembered too, so that the
 * component can tell "no finger on the screen" from "a finger that missed
 * every blob" — recording reads the finger without grabbing anything.
 *
 * Kept apart from MotionComponent because it is the whole of the multitouch
 * bookkeeping and the only part of it that can be tested without a screen.
 */
class TouchGrabs
{
public:
  /** A finger went down, holding `channel` — or nothing, if it missed. */
  void down (int source, std::optional<index_t> channel);

  /** The channel this finger is dragging, if any. */
  std::optional<index_t> channelFor (int source) const;

  /** Whether any finger is holding this channel. A blob already under someone
   *  else's finger is not free to be taken. */
  bool isHeld (index_t channel) const;

  /** A finger lifted. Returns the channel it was holding, which is the one —
   *  and the only one — to hand back to playback. */
  std::optional<index_t> up (int source);

  /** Whether no finger is on the screen at all. */
  bool empty () const;

  /** The finger that has been down longest — the one a recording follows.
   *  Oldest, not lowest-numbered: touch indices are slots that get reused as
   *  fingers come and go, so the lowest one is not the first. */
  std::optional<int> firstSource () const;

  /** Every channel currently held, in no particular order. Disocclusion pushes
   *  the untouched blobs away from all of these. */
  std::vector<index_t> heldChannels () const;

private:
  std::map<int, std::optional<index_t> > _bySource;
  /** Sources in the order they went down; _bySource is ordered by index. */
  std::vector<int> _order;
};

}
