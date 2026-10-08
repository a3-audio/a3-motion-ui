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

#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/FlightVec.hh>
#include <a3-motion-engine/flight/FlightWorld.hh>

#include <array>
#include <cstddef>
#include <optional>

namespace a3
{

/** A game an action asked a pilot for (an action posts it, the game
 *  that starts takes it). `order.game` is set; `leader` is the channel whose
 *  action asked. */
struct GameRequest
{
  PilotOrder order;
  int leader = -1;
  /** When it was posted, on the engine's beat count. post() leaves it to the
   *  caller to set. */
  double postedBeats = 0.;
  /** Counts posts, so a reader tells a new request from one it has seen. */
  unsigned serial = 0;
};

/** What a ship was doing when a game took it, so it can go back to it when
 *  the game ends (a recruited ship returns to what it was doing).
 *  Taken by whoever starts the game. */
struct ShipResume
{
  bool orbiting = false;  // ORBIT, or flying its clip
  int bodyId = noBodyId;  // the group it escorted, or none: patrolling
};

/** Which ships a game takes, and what each of them was doing. A ship not
 *  taken keeps a default resume. */
struct Recruitment
{
  std::array<bool, flightShips> ships{};
  std::array<ShipResume, flightShips> resume{};
};

/** The ships `with` brings into `leader`'s game. \self: the leader alone;
 *  \nearest: the leader and the free ship nearest it on the floor (`where`,
 *  the lower channel on a tie); \all: the leader and every free ship. The
 *  leader always comes -- it is its own action; whether a leader already in
 *  a game may start another is up to whoever starts the game. Pure. */
Recruitment recruit (PilotRecruit with, int leader,
                     std::array<Vec2, flightShips> const &where,
                     std::array<bool, flightShips> const &free,
                     std::array<ShipResume, flightShips> const &now);

/** One pending game request per ship, the newest wins. Owned by the clock
 *  thread; fixed arrays, no allocation. */
class PilotDesk
{
public:
  /** A request whose game is None calls off the leader's pending one; a
   *  request without a game, or for a ship outside the four, is ignored. */
  void post (GameRequest request);
  std::optional<GameRequest> pending (int ship) const;
  /** The pending request, removed -- what whoever starts a game calls. */
  std::optional<GameRequest> take (int ship);

private:
  std::array<std::optional<GameRequest>, flightShips> _pending{};
  unsigned _serial = 0;
};

}
