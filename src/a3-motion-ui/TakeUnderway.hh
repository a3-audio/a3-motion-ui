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

#include <a3-motion-ui/PendingTakes.hh>

#include <memory>
#include <optional>
#include <utility>

namespace a3
{

class Pattern;

/** A take from REC until it ends: its slot, the take, and what the slot held.
 *
 *  One place for what each way out puts back (2026-10-08). The slot can be
 *  filled with something else while the take is underway -- a shape, a clip
 *  with its own figure, a set -- and that used to leave REC reading "end
 *  take" and the band locked, and the next REC put the old clip back over
 *  the new one. Knows nothing about the engine or files, like PendingTakes. */
class TakeUnderway
{
public:
  void begin (index_t channel, index_t slot, std::shared_ptr<Pattern> take,
              SlotContent before);

  bool any () const;
  bool isOn (index_t channel, index_t slot) const;
  bool isOnChannel (index_t channel) const;
  std::optional<std::pair<index_t, index_t> > slot () const;
  std::shared_ptr<Pattern> const &take () const;
  SlotContent const &before () const;

  /** Something else is put in `channel`/`slot`. A take on it ends there,
   *  nothing is put back, and the take is handed out for the engine to stop
   *  or call off; null when no take was on that slot. */
  std::shared_ptr<Pattern> slotReplaced (index_t channel, index_t slot);

  /** Called off before its downbeat: what to put back into the slot -- only
   *  while the slot still holds the take (`inTheSlot`), never over something
   *  put there since. */
  std::optional<SlotContent> calledOff (std::shared_ptr<Pattern> const &inTheSlot);

  /** Ended after it ran: what the slot held, for PendingTakes. */
  SlotContent ended ();

private:
  void clear ();

  std::optional<std::pair<index_t, index_t> > _slot;
  std::shared_ptr<Pattern> _take;
  SlotContent _before;
};

}
