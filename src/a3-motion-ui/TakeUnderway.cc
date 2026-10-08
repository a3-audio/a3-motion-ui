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

#include "TakeUnderway.hh"

namespace a3
{

void
TakeUnderway::begin (index_t channel, index_t slot,
                     std::shared_ptr<Pattern> take, SlotContent before)
{
  _slot = std::make_pair (channel, slot);
  _take = std::move (take);
  _before = std::move (before);
}

bool
TakeUnderway::any () const
{
  return _slot.has_value ();
}

bool
TakeUnderway::isOn (index_t channel, index_t slot) const
{
  return _slot && *_slot == std::make_pair (channel, slot);
}

bool
TakeUnderway::isOnChannel (index_t channel) const
{
  return _slot && _slot->first == channel;
}

std::optional<std::pair<index_t, index_t> >
TakeUnderway::slot () const
{
  return _slot;
}

std::shared_ptr<Pattern> const &
TakeUnderway::take () const
{
  return _take;
}

SlotContent const &
TakeUnderway::before () const
{
  return _before;
}

std::shared_ptr<Pattern>
TakeUnderway::slotReplaced (index_t channel, index_t slot)
{
  if (!isOn (channel, slot))
    return nullptr;

  auto take = _take;
  clear ();
  return take;
}

std::shared_ptr<Pattern>
TakeUnderway::everythingReplaced ()
{
  if (!_slot)
    return nullptr;
  return slotReplaced (_slot->first, _slot->second);
}

std::optional<index_t>
TakeUnderway::refusesANewTake (index_t channel, index_t slot,
                               PendingTakes const &pending) const
{
  if (_slot)
    return _slot->first;
  if (auto const other = pending.pendingOtherThan (channel, slot))
    return other->first;
  return std::nullopt;
}

std::optional<SlotContent>
TakeUnderway::calledOff (std::shared_ptr<Pattern> const &inTheSlot)
{
  auto const stillThere = _slot.has_value () && inTheSlot == _take;
  auto before = _before;
  clear ();
  if (!stillThere)
    return std::nullopt;
  return before;
}

SlotContent
TakeUnderway::ended ()
{
  auto before = _before;
  clear ();
  return before;
}

void
TakeUnderway::clear ()
{
  _slot.reset ();
  _take.reset ();
  _before = SlotContent{};
}

}
