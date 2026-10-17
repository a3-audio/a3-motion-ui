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
#include <a3-motion-engine/flight/PilotFamily.hh>
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
  /** The channel's FREQ knob, 0-1, as the band is heard: front-back and
   *  up-down slaps want a high one. */
  float bandFreq = 0.f;
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

  /** An action's request: a game named outright, or a style the music turns
   *  into a figure (`\auto`: the music's own family). Its leader leaves
   *  whatever game it plays in; the figure opens on the next beat (a game of
   *  the first version at once) and takes the free ships its `with` brings.
   *  False, and nothing changes, when the leader cannot fly or nothing is
   *  asked. */
  bool request (GameRequest const &request, GameShips const &ships, FlightBodies const &bodies,
                MusicCue const &cue, double beats, int beatsPerBar);
  /** Ends the game `ship` plays in, for every ship in it. */
  void endGameOn (int ship);
  void endAll ();

  /** One tick: ships that cannot fly or that the DJ moved leave; games
   *  past their last bar end, and the pilots' own below FLY; the targets
   *  follow their groups; and at FLY, on a new bar or when the music's cue
   *  changes, a pilot may start a figure of the family the music asks for --
   *  one pilot figure at a time, ORBIT ships only, restBars after the last
   *  one. A drop the pilots hit ends a pilot's soft figure on its first tick,
   *  with no rest, so the slap can open on it; a DJ's figure plays on. A
   *  pilot's climb that does not chain into a previewed drop yields
   *  to a drop the room hears: a slap without the snap. */
  void step (GameShips const &ships, FlightBodies const &bodies, MusicCue const &cue,
             PilotLevel level, double beats, int beatsPerBar);

  bool plays (int ship) const;
  /** What `ship` steers for now; nothing when it plays no game, or while its
   *  figure waits for its beat. */
  std::optional<OrbitPoint> steerOf (int ship, double beats, int beatsPerBar) const;
  /** How `ship` stands besides its point (FigureStance); the default when it
   *  plays no game or its figure has not opened. */
  FigureStance stanceOf (int ship, double beats, int beatsPerBar) const;
  /** Takes `ship` alone out of its game; the rest play on. */
  void release (int ship);
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
              FlightBodies const &bodies, MusicCue const &cue, double beats, int beatsPerBar,
              FigureCall const &call);
  /** What the floor offers a soft figure played from `from` against `target`. */
  struct Room
  {
    bool group = false;
    bool grazeFits = false;
  };
  Room roomFor (PilotTarget target, Vec2 from, FlightBodies const &bodies) const;
  std::optional<PilotGame> pressedFigure (PilotOrder const &order, Vec2 leaderAt,
                                          FlightBodies const &bodies, MusicCue const &cue,
                                          double beats, int beatsPerBar);
  FigureCall pressCall (PilotGame game, GameShips const &ships, int leader, double beats,
                        int beatsPerBar) const;
  /** A pilot's figure as the music asks for it: it opens within
   *  cueGraceBeats of the beat its cue was for; a Slap snaps only on a
   *  previewed drop's 1, opens on a downbeat on a drop the room heard, and is
   *  over hardDropBars after the drop's 1. */
  FigureCall pilotsCall (PilotGame game, MusicCue const &cue, GameShips const &ships, int leader,
                         double beats, int beatsPerBar) const;
  /** A climb on its 1 into a previewed drop becomes a slap for a pair at
   *  most, in the same game slot: the drop's first hardDropBars. */
  void chainIntoSlap (int slot, GameShips const &ships, FlightBodies const &bodies,
                      MusicCue const &cue, int beatsPerBar);
  /** A pilot's climb that will not chain, as a drop the room heard asks for
   *  HARD, becomes the pilots' slap on it; it climbs on when it is too late
   *  for one. */
  void yieldToTheDrop (int slot, GameShips const &ships, FlightBodies const &bodies,
                       MusicCue const &cue, double beats, int beatsPerBar);
  /** The game in `slot` replanned as a slap called `call`, for `leader`
   *  and the first other ship still in it at most; the rest are released. */
  void turnIntoSlap (int slot, int leader, GameShips const &ships, FlightBodies const &bodies,
                     MusicCue const &cue, double beats, int beatsPerBar, FigureCall const &call);
  /** The leader of the game in `slot` while it still plays in it, else the
   *  first ship still in it, or -1. */
  int leaderIn (int slot) const;
  /** Whether a slap called `call` leaves room for a step after its opening. */
  bool slapHasRoom (FigureCall const &call) const;
  void leave (int ship);
  /** Ends the game in `slot`; after a pilot's own, the pilots rest. */
  void end (int slot);
  /** Ends the game in `slot` with no rest after it. */
  void clear (int slot);
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
  /** The cue of the last step: a pilot looks again when it moves. */
  MusicCue _lastCue{};
};

}
