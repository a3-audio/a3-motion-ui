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

#include <array>

#include <juce_core/juce_core.h>

namespace a3
{

/** What a recording pass writes where the finger is not.
 *
 *  Named after the automation modes of a sequencer, and meaning the same
 *  things. The device's own recording has always behaved as Touch; the other
 *  two were not choices anyone could make.
 *
 *  This is a second axis, not a replacement for RecordingMode: that one says
 *  when a take ends, this one says what its passes write. */
enum class RecMode
{
  /** Punch-out. A lifted finger writes nothing, so an earlier pass stands
   *  where this one did not go. Rough a shape out, then mend one corner. */
  Touch,

  /** Takes hold at the first touch and keeps writing after the finger lifts,
   *  holding the position it was left at -- to the end of that lap, and no
   *  further. See shouldWriteTick. */
  Latch,

  /** Writes the whole pass, touched or not: the one that clears an old take
   *  out of the way rather than adding to it. That is one pass, not every
   *  pass -- see shouldWriteTick. */
  Write
};

/** The modes the Automation row and both rec-mode keys offer, in the order
 *  they offer them.
 *
 *  Beside the enum rather than inside whoever draws the row: the panel's key,
 *  the bar's key and the menu all step through this list, and a list that
 *  lived in one of them would be a list the other two had to guess at. Read is
 *  absent on purpose -- see where the row is built. */
constexpr std::array<RecMode, 3> recMenuModes{
  RecMode::Touch, RecMode::Latch, RecMode::Write
};

/** Where a mode sits in that list. Falls back to the first rather than to -1:
 *  a mode the list does not contain is a bug in the list, and a key that lands
 *  somewhere valid is easier to notice than one that steps to index zero
 *  forever. */
constexpr int
recMenuIndex (RecMode mode)
{
  for (int i = 0; i < static_cast<int> (recMenuModes.size ()); ++i)
    if (recMenuModes[static_cast<std::size_t> (i)] == mode)
      return i;

  return 0;
}

/** Whether this tick is written.
 *
 *  `hasTouched` is about the take, not the tick: whether the finger has been
 *  down at any point since the take began. It is what separates Latch from
 *  Write, which are otherwise the same. */
/** Where the finger is, and how far the take has run.
 *
 *  `ticksAtLift` is where the write head was when the finger last came up,
 *  and `ticksNow` where it is; both counted from the start of the take, so
 *  they keep counting across laps. `lapTicks` is the take's own length. */
struct FingerHistory
{
  bool down = false;
  bool hasTouched = false;
  long long ticksAtLift = 0;
  long long ticksNow = 0;
  long long lapTicks = 0;
};

/** Whether this tick is written.
 *
 *  **A hold ends with the lap it began in.** Recording runs round and round
 *  inside the take's length, so a hold that outlives its lap comes back to
 *  where the figure is and writes the held position over it, tick by tick.
 *  Found on the device on 2026-09-18: a take of 569 points with 251 of them
 *  the same place, and the next one a single dot -- *"die gezeichnete
 *  trajektorie verkürzt sich wenn keine toucheingabe passiert auf 0 >
 *  reultat aufnahme leer."* Holding to the end of the pass is what Latch is
 *  for; holding past it is an eraser going round. */
bool shouldWriteTick (RecMode mode, FingerHistory const &finger);

/** Whether the take should be drawn over what was in the slot before it.
 *
 *  Touch and Latch leave parts of the earlier take standing, so you have to be
 *  able to see what you are about to write over. Write replaces the whole
 *  pass, and a ghost of the old one beside the new one would say nothing about
 *  what is going to be there. */
bool showsRecordingUnderlay (RecMode mode, bool recording, bool hasPrevious);

/** The name a settings file stores, and the mode it names.
 *
 *  A name that is not one of the three — an older file, or one edited by
 *  hand — falls back to Touch, the behaviour this device has always had. */
juce::String recModeName (RecMode mode);
RecMode recModeFromName (juce::String const &name);

}
