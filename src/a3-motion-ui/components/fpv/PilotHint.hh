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

/** What one action button asks of its pilot, for the hint: its `~game` and
 *  its `~style`, as written. */
struct PadAsk
{
  std::optional<PilotGame> game;
  std::optional<PilotStyle> style;
};

/** Whether the moment fits what `ask` names: a game (not \none) whose
 *  moment is open, else a style of a family whose figures would be played
 *  now. An action that names neither, or only \auto or \none, never fits:
 *  it would fit always and light every pad. */
bool askFits (PadAsk const &ask, FittingGames const &fitting);

/** The action button a pilot lights on its channel at HINT: the lowest one
 *  whose ask fits, while the channel's ship is free to play (it flies its
 *  orbit with a running clip and plays no game). -1: none -- below or above
 *  HINT, in FULL, or nothing fits. */
int hintedButton (PilotLevel level, AppView view, FittingGames const &fitting, bool shipIsFree,
                  std::array<PadAsk, numActionButtons> const &asks);

}
