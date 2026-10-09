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

int
hintedButton (PilotLevel level, AppView view, FittingGames const &fitting, bool shipIsFree,
              std::array<std::optional<PilotGame>, numActionButtons> const &games)
{
  if (level != PilotLevel::Hint || view != AppView::Fpv || !shipIsFree || !fitting.any ())
    return -1;
  for (auto button = 0; button < static_cast<int> (games.size ()); ++button)
    if (auto const &game = games[static_cast<size_t> (button)]; game && fitting.has (*game))
      return button;
  return -1;
}

}
