/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include "LedState.hh"

namespace a3
{

LedState::LedState (std::size_t slotCount)
    : _colours (slotCount), _changed (slotCount, false)
{
}

void
LedState::set (std::size_t slot, juce::Colour colour)
{
  if (slot >= _colours.size ())
    return;

  std::scoped_lock const guard (_lock);
  _colours[slot] = colour;
  _changed[slot] = true;
}

std::vector<LedState::Change>
LedState::takeChanged ()
{
  std::vector<Change> changes;

  std::scoped_lock const guard (_lock);
  for (std::size_t slot = 0; slot < _colours.size (); ++slot)
    {
      if (!_changed[slot])
        continue;

      changes.push_back ({ slot, _colours[slot] });
      _changed[slot] = false;
    }
  return changes;
}

}
