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

#include <a3-motion-engine/flight/ClimbFigure.hh>
#include <a3-motion-engine/flight/FigureMath.hh>
#include <a3-motion-engine/flight/SoftFigures.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
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

std::array<ShipState, flightShips>
statesOf (GameShips const &ships)
{
  std::array<ShipState, flightShips> states{};
  for (auto s = 0; s < flightShips; ++s)
    states[shipIndex (s)] = ships[shipIndex (s)].state;
  return states;
}

/** What every figure is told about the ships: the leader's band, who flies. */
FigureCall
callFor (GameShips const &ships, int leader)
{
  FigureCall call;
  call.bandFreq = ships[shipIndex (leader)].bandFreq;
  for (auto s = 0; s < flightShips; ++s)
    call.flying[shipIndex (s)] = ships[shipIndex (s)].canFly;
  return call;
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
    case PilotGame::Slap:
    case PilotGame::Encircle:
      // A pair at most: a partner mirrors on the opposite side, and a third
      // ship would stand where one of the two stands.
      return with == PilotRecruit::All ? PilotRecruit::Nearest : with;
    case PilotGame::Graze:
      // One line past one group: a second ship on it would meet the first.
      return PilotRecruit::Self;
    case PilotGame::Climb:
      return with;
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
    case PilotGame::Slap:
    case PilotGame::Climb:
    case PilotGame::Graze:
    case PilotGame::Encircle:
      return PilotRecruit::Self;
    }
  return PilotRecruit::Self;
}

/** A figure that meets the drop's 1 itself: the slap, and the climb that
 *  turns into one. The drop pre-empts every other figure a pilot plays. */
bool
meetsTheDrop (PilotGame game)
{
  switch (game)
    {
    case PilotGame::Slap:
    case PilotGame::Climb:
      return true;
    case PilotGame::None:
    case PilotGame::FakeOut:
    case PilotGame::Formation:
    case PilotGame::HideAndSeek:
    case PilotGame::CallAndResponse:
    case PilotGame::Graze:
    case PilotGame::Encircle:
      return false;
    }
  return false;
}

/** Whether two cues name the same moment: a pilot looks again when it moves. */
bool
sameMoment (MusicCue const &a, MusicCue const &b)
{
  return a.section == b.section && a.next == b.next && a.changeBar == b.changeBar
         && a.sectionStartBar == b.sectionStartBar;
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
  if (!isShip (leader) || !ships[shipIndex (leader)].canFly || beatsPerBar <= 0)
    return false;
  auto const game = pressedFigure (request.order, ships[shipIndex (leader)].state.p, bodies, cue,
                                   beats, beatsPerBar);
  if (!game || *game == PilotGame::None)
    return false;

  leave (leader);
  std::array<bool, flightShips> free{};
  for (auto s = 0; s < flightShips; ++s)
    free[shipIndex (s)] = ships[shipIndex (s)].canFly && _slotOf[shipIndex (s)] < 0;
  start (*game, leader, crewFor (*game, request.order.with), request.order.target, false, free,
         ships, bodies, cue, beats, beatsPerBar,
         pressCall (*game, ships, leader, beats, beatsPerBar));
  return true;
}

PilotGames::Room
PilotGames::roomFor (PilotTarget target, Vec2 from, FlightBodies const &bodies) const
{
  Room room;
  auto const id = targetBody (target, from, bodies, _flight);
  room.group = id != noBodyId;
  if (auto const place = bodyPlace (bodies, id))
    room.grazeFits
        = grazeRun (*place, bodyMass (bodies, id, _flight), _flight, _tuning).has_value ();
  return room;
}

std::optional<PilotGame>
PilotGames::pressedFigure (PilotOrder const &order, Vec2 leaderAt, FlightBodies const &bodies,
                           MusicCue const &cue, double beats, int beatsPerBar)
{
  auto const room = roomFor (order.target, leaderAt, bodies);
  if (order.game)
    return playableFigure (*order.game, room.group, room.grazeFits);
  if (!order.style || *order.style == PilotStyle::None)
    return std::nullopt;
  auto const bar = static_cast<long long> (std::floor (beats / beatsPerBar));
  auto const family = *order.style == PilotStyle::Auto
                          ? pilotFamily (cue, bar, Asker::Press, _tuning)
                          : familyOf (*order.style);
  if (!family)
    return std::nullopt;
  auto const game = chooseFigure (*family, Asker::Press, fittingGames (cue, bar, beatsPerBar, _tuning),
                                  room.grazeFits, _dice);
  if (!game)
    return std::nullopt;
  return playableFigure (*game, room.group, room.grazeFits);
}

FigureCall
PilotGames::pressCall (PilotGame game, GameShips const &ships, int leader, double beats,
                       int beatsPerBar) const
{
  auto call = callFor (ships, leader);
  call.openingBeats = openingBeats (game, beats, beatsPerBar, 0.);
  // A press on a 1 hits it with the jump to the front; off the 1 a slap
  // opens on its first side.
  call.snaps = game == PilotGame::Slap && isDownbeat (*call.openingBeats, beatsPerBar);
  return call;
}

void
PilotGames::release (int ship)
{
  if (isShip (ship))
    leave (ship);
}

void
PilotGames::start (PilotGame game, int leader, PilotRecruit with, PilotTarget target,
                   bool byPilot, std::array<bool, flightShips> const &free,
                   GameShips const &ships, FlightBodies const &bodies, MusicCue const &cue,
                   double beats, int beatsPerBar, FigureCall const &call)
{
  auto const slot = freeSlot ();
  if (slot < 0)
    return; // cannot happen: every live game holds a ship of its own

  std::array<Vec2, flightShips> where{};
  std::array<ShipResume, flightShips> now{};
  for (auto s = 0; s < flightShips; ++s)
    {
      where[shipIndex (s)] = ships[shipIndex (s)].state.p;
      now[shipIndex (s)] = ships[shipIndex (s)].now;
    }
  auto const crew = recruit (with, leader, where, free, now);

  auto &running = _games[shipIndex (slot)];
  running.live = true;
  running.byPilot = byPilot;
  running.resume = crew.resume;
  running.plan = planGame (game, leader, crew.ships, statesOf (ships), target, bodies, cue, beats,
                           beatsPerBar, _dice, _flight, _tuning, call);
  for (auto s = 0; s < flightShips; ++s)
    if (crew.ships[shipIndex (s)])
      _slotOf[shipIndex (s)] = slot;
}

void
PilotGames::leave (int ship)
{
  auto const slot = _slotOf[shipIndex (ship)];
  if (slot < 0)
    return;
  _slotOf[shipIndex (ship)] = -1;
  if (std::none_of (_slotOf.begin (), _slotOf.end (), [slot] (int s) { return s == slot; }))
    end (slot);
}

void
PilotGames::end (int slot)
{
  auto const pilots = _games[shipIndex (slot)].live && _games[shipIndex (slot)].byPilot;
  clear (slot);
  if (pilots)
    _restUntilBar = _bar + _tuning.restBars;
}

void
PilotGames::clear (int slot)
{
  for (auto &s : _slotOf)
    if (s == slot)
      s = -1;
  _games[shipIndex (slot)].live = false;
}

void
PilotGames::endGameOn (int ship)
{
  if (isShip (ship) && _slotOf[shipIndex (ship)] >= 0)
    end (_slotOf[shipIndex (ship)]);
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
    if (!_games[shipIndex (slot)].live)
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
      auto const slot = _slotOf[shipIndex (s)];
      if (slot < 0)
        continue;
      if (!ships[shipIndex (s)].canFly
          || !sameDoing (ships[shipIndex (s)].now, _games[shipIndex (slot)].resume[shipIndex (s)]))
        leave (s);
    }

  // The drop's 1 pre-empts a pilot's figure, and takes no rest for it:
  // the slap that hits the drop starts on the same tick.
  auto const dropHits = level == PilotLevel::Fly
                        && pilotFamily (cue, bar, Asker::Pilot, _tuning) == PilotFamily::Hard;
  for (auto slot = 0; slot < flightShips; ++slot)
    {
      auto &running = _games[shipIndex (slot)];
      if (!running.live)
        continue;
      if (dropHits && running.byPilot && !meetsTheDrop (running.plan.game))
        {
          clear (slot);
          continue;
        }
      if (running.plan.game == PilotGame::Climb && running.plan.chainsIntoSlap
          && beats >= running.plan.climaxBeats)
        chainIntoSlap (slot, ships, bodies, cue, beatsPerBar);
      if (running.byPilot && level == PilotLevel::Fly && running.plan.game == PilotGame::Climb
          && !running.plan.chainsIntoSlap)
        yieldToTheDrop (slot, ships, bodies, cue, beats, beatsPerBar);
      if (beats >= running.plan.endBeats || (running.byPilot && level != PilotLevel::Fly))
        {
          end (slot);
          continue;
        }
      if (auto const place = bodyPlace (bodies, running.plan.targetBodyId))
        running.plan.target = *place;
    }

  auto const cueMoved = !sameMoment (cue, _lastCue);
  _lastCue = cue;
  if (level == PilotLevel::Fly && (bar != _lastBar || cueMoved) && bar >= _restUntilBar
      && !aPilotPlays ())
    letAPilotPlay (ships, bodies, cue, bar, beats, beatsPerBar);
  _lastBar = bar;
}

void
PilotGames::chainIntoSlap (int slot, GameShips const &ships, FlightBodies const &bodies,
                           MusicCue const &cue, int beatsPerBar)
{
  auto &running = _games[shipIndex (slot)];
  running.plan.chainsIntoSlap = false; // asked once, on the 1
  auto const climax = running.plan.climaxBeats;
  auto const dropBar = static_cast<long long> (std::llround (climax / beatsPerBar));
  if (!dropOpensOn (cue, dropBar))
    return; // no drop: the climb holds at the top, then lets go

  auto const leader = leaderIn (slot);
  if (leader < 0)
    return;
  auto call = callFor (ships, leader);
  call.openingBeats = climax;
  call.snaps = true;
  call.endByBeats = climax + static_cast<double> (_tuning.hardDropBars) * beatsPerBar;
  turnIntoSlap (slot, leader, ships, bodies, cue, climax, beatsPerBar, call);
}

void
PilotGames::yieldToTheDrop (int slot, GameShips const &ships, FlightBodies const &bodies,
                            MusicCue const &cue, double beats, int beatsPerBar)
{
  if (pilotFamily (cue, _bar, Asker::Pilot, _tuning) != PilotFamily::Hard)
    return;
  auto const leader = leaderIn (slot);
  if (leader < 0)
    return;
  auto const call = pilotsCall (PilotGame::Slap, cue, ships, leader, beats, beatsPerBar);
  if (slapHasRoom (call))
    turnIntoSlap (slot, leader, ships, bodies, cue, beats, beatsPerBar, call);
}

void
PilotGames::turnIntoSlap (int slot, int leader, GameShips const &ships,
                          FlightBodies const &bodies, MusicCue const &cue, double beats,
                          int beatsPerBar, FigureCall const &call)
{
  auto &running = _games[shipIndex (slot)];
  // A slap is a pair at most: the leader and the first other ship still in
  // the game. The plan's crew still names ships a press has taken since.
  std::array<bool, flightShips> crew{};
  crew[shipIndex (leader)] = true;
  auto partners = 0;
  for (auto s = 0; s < flightShips; ++s)
    {
      if (_slotOf[shipIndex (s)] != slot || s == leader)
        continue;
      if (partners++ == 0)
        crew[shipIndex (s)] = true;
      else
        _slotOf[shipIndex (s)] = -1;
    }
  running.plan = planGame (PilotGame::Slap, leader, crew, statesOf (ships),
                           PilotTarget{}, bodies, cue, beats, beatsPerBar, _dice, _flight,
                           _tuning, call);
}

int
PilotGames::leaderIn (int slot) const
{
  auto const leader = _games[shipIndex (slot)].plan.leader;
  if (isShip (leader) && _slotOf[shipIndex (leader)] == slot)
    return leader;
  for (auto s = 0; s < flightShips; ++s)
    if (_slotOf[shipIndex (s)] == slot)
      return s;
  return -1;
}

bool
PilotGames::slapHasRoom (FigureCall const &call) const
{
  auto const opening = call.openingBeats.value_or (0.);
  return call.endByBeats - opening >= static_cast<double> (_tuning.eventSpacingBeats);
}

void
PilotGames::letAPilotPlay (GameShips const &ships, FlightBodies const &bodies,
                           MusicCue const &cue, long long bar, double beats, int beatsPerBar)
{
  auto const family = pilotFamily (cue, bar, Asker::Pilot, _tuning);
  if (!family)
    return;

  std::array<bool, flightShips> free{};
  auto freeShips = 0;
  for (auto s = 0; s < flightShips; ++s)
    {
      free[shipIndex (s)] = ships[shipIndex (s)].canFly && ships[shipIndex (s)].now.orbiting
                            && _slotOf[shipIndex (s)] < 0;
      freeShips += free[shipIndex (s)] ? 1 : 0;
    }
  if (freeShips == 0)
    return;
  auto pick = _dice.nextInt (freeShips);
  auto leader = -1;
  for (auto s = 0; s < flightShips && leader < 0; ++s)
    if (free[shipIndex (s)] && pick-- == 0)
      leader = s;

  auto const room = roomFor (PilotTarget{}, ships[shipIndex (leader)].state.p, bodies);
  // A pilot never encircles: chooseFigure leaves the escort to the DJ,
  // whatever fittingGames says fits.
  auto const game = chooseFigure (*family, Asker::Pilot,
                                  fittingGames (cue, bar, beatsPerBar, _tuning), room.grazeFits,
                                  _dice);
  if (!game)
    return;
  auto const call = pilotsCall (*game, cue, ships, leader, beats, beatsPerBar);
  // Too late in the drop's window for even one step after the opening.
  if (*game == PilotGame::Slap && !slapHasRoom (call))
    return;
  start (*game, leader, pilotsCrew (*game), PilotTarget{}, true, free, ships, bodies, cue, beats,
         beatsPerBar, call);
}

FigureCall
PilotGames::pilotsCall (PilotGame game, MusicCue const &cue, GameShips const &ships, int leader,
                        double beats, int beatsPerBar) const
{
  auto call = callFor (ships, leader);
  call.openingBeats = openingBeats (game, beats, beatsPerBar, _tuning.cueGraceBeats);
  if (game != PilotGame::Slap || cue.sectionStartBar < 0)
    return call;
  auto const bar = static_cast<double> (beatsPerBar);
  auto const dropOne = static_cast<double> (cue.sectionStartBar) * bar;
  // The room hears a drop a bar late, when its 1 has passed: the slap waits
  // for the next downbeat. Only a previewed drop is hit on its 1 with the
  // jump to the front.
  if (!cue.previewed)
    call.openingBeats = std::ceil (*call.openingBeats / bar - 1e-9) * bar;
  call.snaps = cue.previewed && std::abs (*call.openingBeats - dropOne) < 1e-9;
  call.endByBeats = dropOne + static_cast<double> (_tuning.hardDropBars) * bar;
  return call;
}

bool
PilotGames::plays (int ship) const
{
  return isShip (ship) && _slotOf[shipIndex (ship)] >= 0;
}

std::optional<OrbitPoint>
PilotGames::steerOf (int ship, double beats, int beatsPerBar) const
{
  if (!plays (ship))
    return std::nullopt;
  auto const &plan = _games[shipIndex (_slotOf[shipIndex (ship)])].plan;
  // Waiting for its beat, the ship flies on as it was.
  if (beats < plan.startBeats)
    return std::nullopt;
  return figureGoal (plan, ship, beats, beatsPerBar, _tuning);
}

FigureStance
PilotGames::stanceOf (int ship, double beats, int beatsPerBar) const
{
  if (!plays (ship))
    return {};
  return figureStance (_games[shipIndex (_slotOf[shipIndex (ship)])].plan, ship, beats,
                       beatsPerBar, _tuning);
}

std::optional<ShipGame>
PilotGames::gameOf (int ship) const
{
  if (!plays (ship))
    return std::nullopt;
  auto const &running = _games[shipIndex (_slotOf[shipIndex (ship)])];
  return ShipGame{ running.plan.game, running.byPilot, running.plan.leader,
                   running.plan.targetBodyId };
}

}
