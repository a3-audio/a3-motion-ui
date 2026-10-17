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

#include <a3-motion-engine/flight/GameFigures.hh>

#include <array>
#include <initializer_list>

// Shared builders for the figure tests: a crew and a cruising fleet.

namespace a3::figurefixtures
{

/** A crew of just the ships named. */
inline std::array<bool, flightShips>
crewOf (std::initializer_list<int> ships)
{
  std::array<bool, flightShips> crew{};
  for (auto const s : ships)
    crew[static_cast<size_t> (s)] = true;
  return crew;
}

/** Four ships on a circle of 0.7 between the side points (on the
 *  diagonals, so no side is blocked), flying counter-clockwise. */
inline std::array<ShipState, flightShips>
cruising ()
{
  constexpr float d = 0.495f; // 0.7 / sqrt 2
  std::array<Vec2, flightShips> const at{ Vec2{ d, d }, Vec2{ -d, d }, Vec2{ -d, -d },
                                         Vec2{ d, -d } };
  std::array<ShipState, flightShips> ships{};
  for (size_t i = 0; i < ships.size (); ++i)
    ships[i] = { at[i], Vec2{ -at[i].y, at[i].x } * 0.5f };
  return ships;
}

}
