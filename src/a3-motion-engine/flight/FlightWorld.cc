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

#include "FlightWorld.hh"

#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <cmath>

namespace a3
{

namespace
{

size_t
index (int ch)
{
  return static_cast<size_t> (ch);
}

bool
isShip (int ch)
{
  return ch >= 0 && ch < flightShips;
}

/** A point uniformly inside a disc of `radius`. */
Vec2
pointInDisc (juce::Random &dice, float radius)
{
  auto const angle = 2.f * pi<float> () * dice.nextFloat ();
  auto const distance = radius * std::sqrt (dice.nextFloat ());
  return { distance * std::cos (angle), distance * std::sin (angle) };
}

/** The index of the body with `id`, or -1 when there is none (any more). */
int
indexOfBody (FlightBodies const &bodies, int id)
{
  if (id == noBodyId)
    return -1;
  for (auto i = 0; i < bodies.count && i < maxFlightBodies; ++i)
    if (bodies.body[static_cast<size_t> (i)].id == id)
      return i;
  return -1;
}

/** The index of the body a ship may escort under `orders`, or -1: a dead
 *  zone, or an id that names no body (any more), means PATROL. */
int
escortableBody (ShipOrders const &orders, FlightBodies const &bodies)
{
  if (orders.goal != FlightGoal::Escort)
    return -1;
  auto const index = indexOfBody (bodies, orders.bodyId);
  if (index < 0 || bodies.body[static_cast<size_t> (index)].mass <= 0.f)
    return -1;
  return index;
}

/** `bodies` without body `left`. The escorted body's pull is what the escort
 *  orbit stands for, so only the others tug at an escorting ship.
 *
 *  Tried with the pull added (2026-10-08, captureRadius 0.3): away from the
 *  walls the circle holds either way, but next to the rim the part of the
 *  circle outside the room is cut off, the rim pushes the ship inwards and
 *  the group swallows it. Seen from the centre the escort then sweeps 10-13
 *  deg (parked) instead of 58-81 deg (FlightWorld test
 *  AnEscortNearTheRimIsHeardAsACircle). */
FlightBodies
withoutBody (FlightBodies const &bodies, int left)
{
  FlightBodies others;
  for (auto i = 0; i < bodies.count && i < maxFlightBodies; ++i)
    if (i != left)
      others.body[static_cast<size_t> (others.count++)]
          = bodies.body[static_cast<size_t> (i)];
  return others;
}

}

FlightWorld::FlightWorld (juce::int64 seed, FlightTuning const &tuning)
    : _tuning (tuning)
{
  for (auto ch = 0; ch < flightShips; ++ch)
    _dice[index (ch)].setSeed (spreadSeed (seed + ch));
}

void
FlightWorld::launch (int ch, Vec2 p, double beats, int beatsPerBar)
{
  if (!isShip (ch))
    return;

  auto const nearest = nearestOrbitPhase (p, beats, beatsPerBar, _tuning);
  _phaseOffset[index (ch)] = nearest - rabbitSlotPhase (beats, ch, beatsPerBar, _tuning);

  auto const tangent = rabbitAt (beats, ch, beatsPerBar, _tuning,
                                 _phaseOffset[index (ch)]).velocity;
  auto const speed = tangent.getDistanceFromOrigin ();
  auto const cruise = juce::jlimit (_tuning.speedMin, _tuning.speedMax, speed);
  auto const velocity = speed > 0.f ? tangent * (cruise / speed)
                                    : Vec2{ cruise, 0.f };
  _ships[index (ch)] = { p, velocity };
  _escort[index (ch)] = {};
}

void
FlightWorld::step (std::array<ShipOrders, flightShips> const &orders,
                   FlightBodies const &bodies, double beats, int beatsPerBar,
                   float pulse, float dt)
{
  redrawWanderOnANewBar (beats, beatsPerBar);

  // Every ship's pulls are taken from where the others were before this
  // tick, so the order the four are stepped in does not matter.
  std::array<ShipForces, flightShips> forces{};
  for (auto ch = 0; ch < flightShips; ++ch)
    {
      if (!orders[index (ch)].flying)
        continue;
      auto const escorted = followOrders (ch, orders[index (ch)], bodies, beats);
      auto const &ship = _ships[index (ch)];
      FlightBodies const pulling
          = escorted < 0 ? bodies : withoutBody (bodies, escorted);
      forces[index (ch)] = { goalFor (ch, bodies, escorted, beats, beatsPerBar),
                             gravityAt (ship.p, pulling, pulse, _tuning)
                                 + deadZonePush (ship.p, bodies, _tuning),
                             separationOf (ch, orders) };
    }

  for (auto ch = 0; ch < flightShips; ++ch)
    if (orders[index (ch)].flying)
      _ships[index (ch)]
          = stepShip (_ships[index (ch)], forces[index (ch)], dt, _tuning);
}

ShipState const &
FlightWorld::ship (int ch) const
{
  return _ships[index (juce::jlimit (0, flightShips - 1, ch))];
}

int
FlightWorld::followOrders (int ch, ShipOrders const &orders,
                           FlightBodies const &bodies, double beats)
{
  auto &leg = _escort[index (ch)];
  auto const body = escortableBody (orders, bodies);
  if (body < 0)
    {
      leg = {};
      return -1;
    }
  auto const &escorted = bodies.body[static_cast<size_t> (body)];
  if (escorted.id == leg.bodyId)
    return body;

  auto const &ship = _ships[index (ch)];
  auto const fromBody = ship.p - escorted.at;
  auto const cross = fromBody.x * ship.v.y - fromBody.y * ship.v.x;
  leg = { escorted.id, std::atan2 (fromBody.y, fromBody.x),
          cross < 0.f ? -1.f : 1.f, beats };
  return body;
}

OrbitPoint
FlightWorld::escortGoal (int ch, FlightBody const &body, double beats,
                         int beatsPerBar) const
{
  auto const &leg = _escort[index (ch)];
  auto const radius = _tuning.captureRadius * std::sqrt (body.mass);
  auto const lap = static_cast<double> (_tuning.captureLapBars) * beatsPerBar;
  // no lap (<= 0 bars): the goal stands where the ship came in, as a
  // rabbit stands at its slot when orbitLapBars is not positive
  auto const turns = lap > 0. ? (beats - leg.startBeats) / lap : 0.;
  auto const angle = leg.startAngle
                     + leg.direction * 2.f * pi<float> ()
                           * static_cast<float> (turns - std::floor (turns));
  auto const rate = lap > 0. ? leg.direction * 2.f * pi<float> ()
                                   / static_cast<float> (lap)
                             : 0.f;
  Vec2 const out{ std::cos (angle), std::sin (angle) };
  Vec2 const along{ -out.y, out.x };
  return { body.at + out * radius, along * (radius * rate) };
}

OrbitPoint
FlightWorld::goalFor (int ch, FlightBodies const &bodies, int escortedIndex,
                      double beats, int beatsPerBar) const
{
  if (escortedIndex >= 0 && beatsPerBar > 0)
    return escortGoal (ch, bodies.body[static_cast<size_t> (escortedIndex)],
                       beats, beatsPerBar);

  auto goal = rabbitAt (beats, ch, beatsPerBar, _tuning, _phaseOffset[index (ch)]);
  goal.at += _wander[index (ch)];
  return goal;
}

Vec2
FlightWorld::separationOf (int ch,
                           std::array<ShipOrders, flightShips> const &orders) const
{
  auto const coreSquared = softeningSquared (_tuning.separationSoftening);
  auto const &p = _ships[index (ch)].p;
  Vec2 push;
  for (auto other = 0; other < flightShips; ++other)
    {
      if (other == ch || !orders[index (other)].flying)
        continue;
      auto const away = p - _ships[index (other)].p;
      push += away
              * (_tuning.separation
                 / (away.getDistanceSquaredFromOrigin () + coreSquared));
    }
  return push;
}

void
FlightWorld::redrawWanderOnANewBar (double beats, int beatsPerBar)
{
  if (beatsPerBar <= 0)
    return;
  auto const bar = static_cast<long long> (std::floor (beats / beatsPerBar));
  if (bar == _lastWanderBar)
    return;
  _lastWanderBar = bar;
  for (auto ch = 0; ch < flightShips; ++ch)
    _wander[index (ch)] = pointInDisc (_dice[index (ch)], _tuning.wanderRadius);
}

}
