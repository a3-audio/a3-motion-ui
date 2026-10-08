/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

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

double
lapBeats (int beatsPerBar, FlightTuning const &tuning)
{
  return static_cast<double> (tuning.orbitLapBars) * beatsPerBar;
}

/** The phase channel `ch`'s rabbit would have at `beats` without an offset. */
float
slotPhase (int ch, double beats, int beatsPerBar, FlightTuning const &tuning)
{
  auto const phase = beats / lapBeats (beatsPerBar, tuning) - 0.25 * ch;
  return static_cast<float> (phase - std::floor (phase));
}

/** A point uniformly inside a disc of `radius`. */
Vec2
pointInDisc (juce::Random &dice, float radius)
{
  auto const angle = 2.f * pi<float> () * dice.nextFloat ();
  auto const distance = radius * std::sqrt (dice.nextFloat ());
  return { distance * std::cos (angle), distance * std::sin (angle) };
}

}

FlightWorld::FlightWorld (juce::int64 seed)
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
  _phaseOffset[index (ch)] = nearest - slotPhase (ch, beats, beatsPerBar, _tuning);

  auto const tangent = rabbitAt (beats, ch, beatsPerBar, _tuning,
                                 _phaseOffset[index (ch)]).velocity;
  auto const speed = tangent.getDistanceFromOrigin ();
  auto const cruise = juce::jlimit (_tuning.speedMin, _tuning.speedMax, speed);
  auto const velocity = speed > 0.f ? tangent * (cruise / speed)
                                    : Vec2{ cruise, 0.f };
  _ships[index (ch)] = { p, velocity };
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
      auto const &ship = _ships[index (ch)];
      forces[index (ch)] = { goalFor (ch, beats, beatsPerBar),
                             gravityAt (ship.p, bodies, pulse, _tuning),
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

OrbitPoint
FlightWorld::goalFor (int ch, double beats, int beatsPerBar) const
{
  auto goal = rabbitAt (beats, ch, beatsPerBar, _tuning, _phaseOffset[index (ch)]);
  goal.at += _wander[index (ch)];
  return goal;
}

Vec2
FlightWorld::separationOf (int ch,
                           std::array<ShipOrders, flightShips> const &orders) const
{
  auto const softeningSquared = _tuning.softening * _tuning.softening;
  auto const &p = _ships[index (ch)].p;
  Vec2 push;
  for (auto other = 0; other < flightShips; ++other)
    {
      if (other == ch || !orders[index (other)].flying)
        continue;
      auto const away = p - _ships[index (other)].p;
      push += away
              * (_tuning.separation
                 / (away.getDistanceSquaredFromOrigin () + softeningSquared));
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
