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

#include "PilotHint.hh"

namespace a3
{

bool
askFits (PadAsk const &ask, FittingGames const &fitting)
{
  if (ask.game)
    return *ask.game != PilotGame::None && fitting.has (*ask.game);
  if (!ask.style)
    return false;
  auto const family = familyOf (*ask.style);
  return family && fitting.fits (*family);
}

int
hintedButton (PilotLevel level, AppView view, FittingGames const &fitting, bool shipIsFree,
              std::array<PadAsk, numActionButtons> const &asks)
{
  if (level != PilotLevel::Hint || view != AppView::Fpv || !shipIsFree || !fitting.any ())
    return -1;
  for (auto button = 0; button < static_cast<int> (asks.size ()); ++button)
    if (askFits (asks[static_cast<size_t> (button)], fitting))
      return button;
  return -1;
}

}
