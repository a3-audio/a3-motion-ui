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

#pragma once

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/flight/GameMoments.hh>
#include <a3-motion-engine/flight/PilotLevel.hh>
#include <a3-motion-ui/components/AppView.hh>

#include <array>
#include <optional>

namespace a3
{

/** The action button a pilot lights on its channel at HINT: the lowest one
 *  whose script names a game that fits the moment, while the channel's ship
 *  is free to play (it flies its orbit with a running clip and plays no
 *  game). Only actions carrying a `~game` light, and `\none` never does.
 *  -1: none -- below or above HINT, in FULL, or nothing fits. */
int hintedButton (PilotLevel level, AppView view, FittingGames const &fitting, bool shipIsFree,
                  std::array<std::optional<PilotGame>, numActionButtons> const &games);

}
