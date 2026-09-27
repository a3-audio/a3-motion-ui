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

#include <JuceHeader.h>

#include <a3-motion-engine/util/Types.hh>

#include <optional>
#include <vector>

namespace a3
{

/** A clip's place: which channel, which of its slots. */
struct SlotRef
{
  index_t channel;
  index_t slot;

  bool
  operator== (SlotRef const &other) const
  {
    return channel == other.channel && slot == other.slot;
  }
};

/** Every slot whose action is `file`, in channel then slot order. Saving a
 *  script re-runs it on all of them, not only the shown one. */
std::vector<SlotRef>
slotsFiring (juce::File const &file,
             std::vector<std::vector<juce::File>> const &slotFiles);

/** Whether the FILES list waits -- a row tap, Rename, Delete -- because the
 *  script beside it has changes that are neither saved nor cancelled. */
bool listWaitsFor (bool scriptHasUnsavedChanges);

/** Which clip a Save as points at the copy: the one EDIT on ACTION came from,
 *  and nobody when FILES was opened from its own key. */
std::optional<SlotRef> slotToRepoint (std::optional<SlotRef> editOrigin);

/** What a copy is named after: the file it came from ("Bloom" gives
 *  "Bloom 2" through freeFileIn), or "Action" for one with no origin. */
juce::String copyBaseFor (juce::File const &from);

}
