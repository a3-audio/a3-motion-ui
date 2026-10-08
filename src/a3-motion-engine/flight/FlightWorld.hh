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

#pragma once

#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-engine/flight/ShipDynamics.hh>

#include <JuceHeader.h>

#include <array>

namespace a3
{

constexpr int flightShips = 4;

/** What a ship steers for: its rabbit on the big path, or a point circling
 *  one of the bodies. */
enum class FlightGoal
{
  Patrol,
  Escort
};

/** One channel's orders, as the engine reads them each tick. */
struct ShipOrders
{
  bool flying = false;
  FlightGoal goal = FlightGoal::Patrol;
  int body = -1;
};

/** The four ships in one gravity field.
 *
 *  Pure and deterministic: time comes in as beats, randomness only from the
 *  seed. `step` allocates nothing and locks nothing, so the clock thread can
 *  own a FlightWorld outright. */
class FlightWorld
{
public:
  explicit FlightWorld (juce::int64 seed);

  /** Put ship `ch` into the air at `p` (CLIP -> ORBIT): velocity along the
   *  path's tangent at cruise speed, rabbit at the nearest phase. */
  void launch (int ch, Vec2 p, double beats, int beatsPerBar);

  /** One tick for every flying ship. No allocation. */
  void step (std::array<ShipOrders, flightShips> const &orders,
             FlightBodies const &bodies, double beats, int beatsPerBar,
             float pulse, float dt);

  ShipState const &ship (int ch) const;

private:
  OrbitPoint goalFor (int ch, double beats, int beatsPerBar) const;
  Vec2 separationOf (int ch,
                     std::array<ShipOrders, flightShips> const &orders) const;
  void redrawWanderOnANewBar (double beats, int beatsPerBar);

  FlightTuning _tuning;
  std::array<ShipState, flightShips> _ships{};
  std::array<float, flightShips> _phaseOffset{}; // set by launch: rabbit starts at the nearest phase
  std::array<Vec2, flightShips> _wander{};
  std::array<juce::Random, flightShips> _dice;
  long long _lastWanderBar = -1;
};

}
