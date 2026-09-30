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

#include "PanelOwnership.hh"

namespace a3
{

void
PanelOwnership::setOwnedByKeyboard (bool owned)
{
  _ownedByKeyboard = owned;
}

PanelRoute
PanelOwnership::route (int button, bool pressed)
{
  if (pressed)
    {
      if (!_ownedByKeyboard)
        return PanelRoute::Panel;
      _typing.insert (button);
      return PanelRoute::Keyboard;
    }

  // A release goes where its press went.
  return _typing.erase (button) > 0 ? PanelRoute::Keyboard : PanelRoute::Panel;
}

}
