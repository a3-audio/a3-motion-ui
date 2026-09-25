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

#include <a3-motion-engine/util/Types.hh>

#include <JuceHeader.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace a3
{

class Pattern;

/** What a slot holds, as far as a set and a Discard care: the take and the
 *  clip file its values came from. */
struct SlotContent
{
  std::shared_ptr<Pattern> pattern;
  juce::File clipFile;
};

/** The takes nobody has saved yet, and what each slot held before its take.
 *
 *  A take is written only when somebody says so. Until then it plays like any
 *  other clip, and it is dropped only when something *replaces* it -- a new
 *  take, a shape dropped on the slot, a set loaded, a restart. Never on a
 *  timer and never because the performer moved on: a good take at 2 a.m. is
 *  exactly the one nobody remembers to save straight away.
 *
 *  Knows nothing about files or the engine, so every rule here is tested
 *  without a window. See the spec in .claude/notes/rec-save-discard.md. */
class PendingTakes
{
public:
  PendingTakes (std::size_t numChannels, std::size_t numSlots);

  /** A take has ended in this slot with something written. A slot that is
   *  already pending keeps its *original* before: Discard always goes back to
   *  the last saved state, and a second take that comes to nothing leaves the
   *  first one standing. */
  void begin (index_t channel, index_t slot, SlotContent before);
  bool isPending (index_t channel, index_t slot) const;

  /** What a set writes for this slot: `before` while it is pending, the slot's
   *  own content otherwise. A set names only what is on disk. */
  SlotContent forSet (index_t channel, index_t slot,
                      SlotContent current) const;

  /** Discarded or replaced: the mark goes, and what the slot held comes back
   *  for whoever wants to put it back. Empty for a slot that was not pending. */
  SlotContent resolve (index_t channel, index_t slot);
  /** Saved: the mark goes and the take stays. */
  void clear (index_t channel, index_t slot);
  void clearAll ();

  /** One press on DISCARD. The first arms, the second on the same slot
   *  confirms -- true means "discard now". A slot without a take cannot be
   *  armed. */
  bool pressDiscard (index_t channel, index_t slot);
  bool isDiscardArmed (index_t channel, index_t slot) const;
  /** Anything else touched. */
  void disarm ();

private:
  bool inRange (index_t channel, index_t slot) const;

  std::size_t _numSlots;
  std::vector<std::optional<SlotContent> > _before;
  std::optional<std::pair<index_t, index_t> > _armed;
};

}
