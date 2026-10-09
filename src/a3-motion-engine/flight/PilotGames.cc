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

#include "PilotGames.hh"

#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
size_t
at (int i)
{
  return static_cast<size_t> (i);
}

bool
isShip (int ship)
{
  return ship >= 0 && ship < flightShips;
}

bool
sameDoing (ShipResume const &a, ShipResume const &b)
{
  return a.orbiting == b.orbiting && a.bodyId == b.bodyId;
}

/** Whom an action's game takes: call & response is a pair whatever the
 *  script says; the others take what `with` brings. */
PilotRecruit
crewFor (PilotGame game, PilotRecruit with)
{
  switch (game)
    {
    case PilotGame::CallAndResponse:
      return PilotRecruit::Nearest;
    case PilotGame::None:
    case PilotGame::FakeOut:
    case PilotGame::Formation:
    case PilotGame::HideAndSeek:
      return with;
    }
  return with;
}

/** Whom a pilot brings to a game of its own. */
PilotRecruit
pilotsCrew (PilotGame game)
{
  switch (game)
    {
    case PilotGame::Formation:
      return PilotRecruit::All;
    case PilotGame::CallAndResponse:
      return PilotRecruit::Nearest;
    case PilotGame::None:
    case PilotGame::FakeOut:
    case PilotGame::HideAndSeek:
      return PilotRecruit::Self;
    }
  return PilotRecruit::Self;
}

/** The game a pilot picks from those that fit: fake-out or formation on a
 *  throw when both do. */
std::optional<PilotGame>
pilotsChoice (FittingGames const &fitting, juce::Random &dice)
{
  if (fitting.fakeOut && fitting.formation)
    return dice.nextBool () ? PilotGame::Formation : PilotGame::FakeOut;
  if (fitting.fakeOut)
    return PilotGame::FakeOut;
  if (fitting.formation)
    return PilotGame::Formation;
  if (fitting.hideAndSeek)
    return PilotGame::HideAndSeek;
  if (fitting.callAndResponse)
    return PilotGame::CallAndResponse;
  return std::nullopt;
}
}

PilotGames::PilotGames (juce::int64 seed, FlightTuning const &flight, GameTuning const &tuning)
    : _flight (flight), _tuning (tuning), _dice (spreadSeed (seed))
{
  _slotOf.fill (-1);
}

bool
PilotGames::request (GameRequest const &request, GameShips const &ships,
                     FlightBodies const &bodies, MusicCue const &cue, double beats,
                     int beatsPerBar)
{
  auto const leader = request.leader;
  if (!isShip (leader) || !request.order.game || *request.order.game == PilotGame::None
      || !ships[at (leader)].canFly)
    return false;

  leave (leader);
  std::array<bool, flightShips> free{};
  for (auto s = 0; s < flightShips; ++s)
    free[at (s)] = ships[at (s)].canFly && _slotOf[at (s)] < 0;
  auto const game = *request.order.game;
  start (game, leader, crewFor (game, request.order.with), request.order.target, false, free,
         ships, bodies, cue, beats, beatsPerBar);
  return true;
}

void
PilotGames::start (PilotGame game, int leader, PilotRecruit with, PilotTarget target,
                   bool byPilot, std::array<bool, flightShips> const &free,
                   GameShips const &ships, FlightBodies const &bodies, MusicCue const &cue,
                   double beats, int beatsPerBar)
{
  auto const slot = freeSlot ();
  if (slot < 0)
    return; // cannot happen: every live game holds a ship of its own

  std::array<Vec2, flightShips> where{};
  std::array<ShipResume, flightShips> now{};
  std::array<ShipState, flightShips> states{};
  for (auto s = 0; s < flightShips; ++s)
    {
      where[at (s)] = ships[at (s)].state.p;
      now[at (s)] = ships[at (s)].now;
      states[at (s)] = ships[at (s)].state;
    }
  auto const crew = recruit (with, leader, where, free, now);

  auto &running = _games[at (slot)];
  running.live = true;
  running.byPilot = byPilot;
  running.resume = crew.resume;
  running.plan = planGame (game, leader, crew.ships, states, target, bodies, cue, beats,
                           beatsPerBar, _dice, _flight, _tuning);
  for (auto s = 0; s < flightShips; ++s)
    if (crew.ships[at (s)])
      _slotOf[at (s)] = slot;
}

void
PilotGames::leave (int ship)
{
  auto const slot = _slotOf[at (ship)];
  if (slot < 0)
    return;
  _slotOf[at (ship)] = -1;
  if (std::none_of (_slotOf.begin (), _slotOf.end (), [slot] (int s) { return s == slot; }))
    end (slot);
}

void
PilotGames::end (int slot)
{
  auto &running = _games[at (slot)];
  if (!running.live)
    return;
  for (auto &s : _slotOf)
    if (s == slot)
      s = -1;
  running.live = false;
  if (running.byPilot)
    _restUntilBar = _bar + _tuning.restBars;
}

void
PilotGames::endGameOn (int ship)
{
  if (isShip (ship) && _slotOf[at (ship)] >= 0)
    end (_slotOf[at (ship)]);
}

void
PilotGames::endAll ()
{
  for (auto slot = 0; slot < flightShips; ++slot)
    end (slot);
}

int
PilotGames::freeSlot () const
{
  for (auto slot = 0; slot < flightShips; ++slot)
    if (!_games[at (slot)].live)
      return slot;
  return -1;
}

bool
PilotGames::aPilotPlays () const
{
  return std::any_of (_games.begin (), _games.end (),
                      [] (Running const &r) { return r.live && r.byPilot; });
}

void
PilotGames::step (GameShips const &ships, FlightBodies const &bodies, MusicCue const &cue,
                  PilotLevel level, double beats, int beatsPerBar)
{
  if (beatsPerBar <= 0)
    return;
  auto const bar = static_cast<long long> (std::floor (beats / beatsPerBar));
  _bar = bar;

  for (auto s = 0; s < flightShips; ++s)
    {
      auto const slot = _slotOf[at (s)];
      if (slot < 0)
        continue;
      if (!ships[at (s)].canFly
          || !sameDoing (ships[at (s)].now, _games[at (slot)].resume[at (s)]))
        leave (s);
    }

  for (auto slot = 0; slot < flightShips; ++slot)
    {
      auto &running = _games[at (slot)];
      if (!running.live)
        continue;
      if (beats >= running.plan.endBeats || (running.byPilot && level != PilotLevel::Fly))
        {
          end (slot);
          continue;
        }
      if (auto const place = bodyPlace (bodies, running.plan.targetBodyId))
        running.plan.target = *place;
    }

  if (level == PilotLevel::Fly && bar != _lastBar && bar >= _restUntilBar && !aPilotPlays ())
    letAPilotPlay (ships, bodies, cue, bar, beats, beatsPerBar);
  _lastBar = bar;
}

void
PilotGames::letAPilotPlay (GameShips const &ships, FlightBodies const &bodies,
                           MusicCue const &cue, long long bar, double beats, int beatsPerBar)
{
  auto const game = pilotsChoice (fittingGames (cue, bar, beatsPerBar, _tuning), _dice);
  if (!game)
    return;

  std::array<bool, flightShips> free{};
  auto freeShips = 0;
  for (auto s = 0; s < flightShips; ++s)
    {
      free[at (s)] = ships[at (s)].canFly && ships[at (s)].now.orbiting && _slotOf[at (s)] < 0;
      freeShips += free[at (s)] ? 1 : 0;
    }
  if (freeShips == 0)
    return;

  auto pick = _dice.nextInt (freeShips);
  auto leader = -1;
  for (auto s = 0; s < flightShips && leader < 0; ++s)
    if (free[at (s)] && pick-- == 0)
      leader = s;
  start (*game, leader, pilotsCrew (*game), PilotTarget{}, true, free, ships, bodies, cue,
         beats, beatsPerBar);
}

bool
PilotGames::plays (int ship) const
{
  return isShip (ship) && _slotOf[at (ship)] >= 0;
}

std::optional<OrbitPoint>
PilotGames::steerOf (int ship, double beats, int beatsPerBar) const
{
  if (!plays (ship))
    return std::nullopt;
  return figureGoal (_games[at (_slotOf[at (ship)])].plan, ship, beats, beatsPerBar, _tuning);
}

std::optional<ShipGame>
PilotGames::gameOf (int ship) const
{
  if (!plays (ship))
    return std::nullopt;
  auto const &running = _games[at (_slotOf[at (ship)])];
  return ShipGame{ running.plan.game, running.byPilot, running.plan.leader };
}

}
