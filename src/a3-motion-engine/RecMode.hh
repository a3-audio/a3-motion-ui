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
   *  holding the position it was left at, until the take ends. */
  Latch,

  /** Writes the whole pass, touched or not. The one that clears an old take
   *  out of the way rather than adding to it. */
  Write
};

/** Whether this tick is written.
 *
 *  `hasTouched` is about the take, not the tick: whether the finger has been
 *  down at any point since the take began. It is what separates Latch from
 *  Write, which are otherwise the same. */
bool shouldWriteTick (RecMode mode, bool fingerDown, bool hasTouched);

/** The name a settings file stores, and the mode it names.
 *
 *  A name that is not one of the three — an older file, or one edited by
 *  hand — falls back to Touch, the behaviour this device has always had. */
juce::String recModeName (RecMode mode);
RecMode recModeFromName (juce::String const &name);

}
