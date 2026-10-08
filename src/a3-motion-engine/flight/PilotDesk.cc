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

#include "PilotDesk.hh"

namespace a3
{

namespace
{
bool
isShip (int ship)
{
  return ship >= 0 && ship < flightShips;
}

size_t
index (int ship)
{
  return static_cast<size_t> (ship);
}

/** The free ship nearest `leader`, the lower channel on a tie; -1 if none. */
int
nearestFree (int leader, std::array<Vec2, flightShips> const &where,
             std::array<bool, flightShips> const &free)
{
  auto best = -1;
  auto bestDistance = 0.f;
  for (auto ship = 0; ship < flightShips; ++ship)
    {
      if (ship == leader || !free[index (ship)])
        continue;
      auto const distance = where[index (ship)].getDistanceFrom (where[index (leader)]);
      if (best < 0 || distance < bestDistance)
        {
          best = ship;
          bestDistance = distance;
        }
    }
  return best;
}
}

Recruitment
recruit (PilotRecruit with, int leader,
         std::array<Vec2, flightShips> const &where,
         std::array<bool, flightShips> const &free,
         std::array<ShipResume, flightShips> const &now)
{
  Recruitment out;
  if (!isShip (leader))
    return out;

  out.ships[index (leader)] = true;
  switch (with)
    {
    case PilotRecruit::Self:
      break;
    case PilotRecruit::Nearest:
      if (auto const other = nearestFree (leader, where, free); other >= 0)
        out.ships[index (other)] = true;
      break;
    case PilotRecruit::All:
      for (auto ship = 0; ship < flightShips; ++ship)
        if (free[index (ship)])
          out.ships[index (ship)] = true;
      break;
    }

  for (auto ship = 0; ship < flightShips; ++ship)
    if (out.ships[index (ship)])
      out.resume[index (ship)] = now[index (ship)];
  return out;
}

void
PilotDesk::post (GameRequest request)
{
  if (!isShip (request.leader) || !request.order.game)
    return;
  auto &slot = _pending[index (request.leader)];
  if (*request.order.game == PilotGame::None)
    {
      slot.reset ();
      return;
    }
  request.serial = ++_serial;
  slot = request;
}

std::optional<GameRequest>
PilotDesk::pending (int ship) const
{
  if (!isShip (ship))
    return std::nullopt;
  return _pending[index (ship)];
}

std::optional<GameRequest>
PilotDesk::take (int ship)
{
  if (!isShip (ship))
    return std::nullopt;
  auto taken = _pending[index (ship)];
  _pending[index (ship)].reset ();
  return taken;
}

}
