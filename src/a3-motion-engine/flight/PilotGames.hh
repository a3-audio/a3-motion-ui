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
#include <a3-motion-engine/flight/PilotDesk.hh>
#include <a3-motion-engine/flight/PilotLevel.hh>

#include <array>
#include <optional>

namespace a3
{

/** The game a ship plays in: which, who leads it, whether a pilot started
 *  it at FLY or the DJ did with an action, and the group it is played
 *  against (noBodyId for a game played against none). The flight spares a
 *  ship in a game that group's pull, as it spares an escort its own: the
 *  figure already stands for it. */
struct ShipGame
{
  PilotGame game = PilotGame::None;
  bool byPilot = false;
  int leader = -1;
  int target = noBodyId;
};

/** One ship as the games see it this tick. */
struct GameShip
{
  /** A clip runs on the channel and no finger or take holds it. */
  bool canFly = false;
  /** The DJ's mode and escort for it, as they stand now -- never what a
   *  game makes of them. */
  ShipResume now;
  /** Where it is on the floor and how it moves; for a ship on its clip,
   *  where it would launch. */
  ShipState state;
};
using GameShips = std::array<GameShip, flightShips>;

/** The games being played. A game never moves a ship: it names, per ship
 *  and tick, the point the flight steers for (steerOf). A ship plays in one
 *  game at most; a recruiting game takes only ships in none. A ship whose
 *  DJ changes its mode or escort, or that can no longer fly, leaves its
 *  game at once and goes back to what the DJ has it doing.
 *
 *  Pure and deterministic (seeded dice), fixed size; the clock thread owns
 *  it outright and nothing here allocates or locks. */
class PilotGames
{
public:
  explicit PilotGames (juce::int64 seed, FlightTuning const &flight = {},
                       GameTuning const &tuning = {});

  /** An action's request: its leader leaves whatever game it plays in, and
   *  the game starts on it and the free ships its `with` brings (call &
   *  response: a pair whatever it says). False, and nothing changes, when
   *  the leader cannot fly or the request names no game to play. */
  bool request (GameRequest const &request, GameShips const &ships, FlightBodies const &bodies,
                MusicCue const &cue, double beats, int beatsPerBar);
  /** Ends the game `ship` plays in, for every ship in it. */
  void endGameOn (int ship);
  void endAll ();

  /** One tick: ships that cannot fly or that the DJ moved leave; games
   *  past their last bar end, and the pilots' own below FLY; the targets
   *  follow their groups; and at FLY, on a new bar, a pilot may start a game
   *  whose moment has opened -- one pilot game at a time, ORBIT ships only,
   *  restBars after the last one. */
  void step (GameShips const &ships, FlightBodies const &bodies, MusicCue const &cue,
             PilotLevel level, double beats, int beatsPerBar);

  bool plays (int ship) const;
  /** What `ship` steers for now; nothing when it plays no game. */
  std::optional<OrbitPoint> steerOf (int ship, double beats, int beatsPerBar) const;
  std::optional<ShipGame> gameOf (int ship) const;

private:
  struct Running
  {
    bool live = false;
    bool byPilot = false;
    GamePlan plan;
    std::array<ShipResume, flightShips> resume{};
  };

  void start (PilotGame game, int leader, PilotRecruit with, PilotTarget target, bool byPilot,
              std::array<bool, flightShips> const &free, GameShips const &ships,
              FlightBodies const &bodies, MusicCue const &cue, double beats, int beatsPerBar);
  void leave (int ship);
  void end (int slot);
  int freeSlot () const;
  bool aPilotPlays () const;
  void letAPilotPlay (GameShips const &ships, FlightBodies const &bodies, MusicCue const &cue,
                      long long bar, double beats, int beatsPerBar);

  FlightTuning _flight;
  GameTuning _tuning;
  juce::Random _dice;
  std::array<Running, flightShips> _games{};
  /** Per ship, the slot of the game it plays in, or -1. */
  std::array<int, flightShips> _slotOf{};
  long long _bar = 0;
  long long _lastBar = -1;
  long long _restUntilBar = 0;
};

}
