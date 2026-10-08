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

/** One channel's orders, as the engine reads them each tick. `bodyId` is
 *  the escorted body's FlightBody::id, never its index. */
struct ShipOrders
{
  bool flying = false;
  FlightGoal goal = FlightGoal::Patrol;
  int bodyId = noBodyId;
};

/** The four ships in one gravity field.
 *
 *  Pure and deterministic: time comes in as beats, randomness only from the
 *  seed. `step` allocates nothing and locks nothing, so the clock thread can
 *  own a FlightWorld outright. */
class FlightWorld
{
public:
  explicit FlightWorld (juce::int64 seed, FlightTuning const &tuning = {});

  /** Put ship `ch` into the air at `p` (CLIP -> ORBIT): velocity along the
   *  path's tangent at cruise speed, rabbit at the nearest phase. */
  void launch (int ch, Vec2 p, double beats, int beatsPerBar);

  /** One tick for every flying ship. No allocation. */
  void step (std::array<ShipOrders, flightShips> const &orders,
             FlightBodies const &bodies, double beats, int beatsPerBar,
             float pulse, float dt);

  ShipState const &ship (int ch) const;

  /** The breath (see Breath.hh): while on, every ship stands still through
   *  the last beat of each bar, velocity kept, and its rabbit runs on, so
   *  the restart on the one is a short chase. Off by default. */
  void setBreathing (bool on) { _breathing = on; }
  bool breathing () const { return _breathing; }

private:
  /** How ship `ch` circles the body it escorts, fixed at the first step
   *  after the order: from the angle it is at, in the sense it is flying. */
  struct EscortLeg
  {
    int bodyId = noBodyId; // noBodyId: patrolling
    float startAngle = 0.f;
    float direction = 1.f; // +1 counter-clockwise, -1 clockwise
    double startBeats = 0.;
  };

  /** Starts, keeps or ends ship `ch`'s escort leg; returns the index of
   *  the body it escorts this tick, or -1. */
  int followOrders (int ch, ShipOrders const &orders,
                    FlightBodies const &bodies, double beats);
  OrbitPoint goalFor (int ch, FlightBodies const &bodies, int escortedIndex,
                      double beats, int beatsPerBar) const;
  OrbitPoint escortGoal (int ch, FlightBody const &body, double beats,
                         int beatsPerBar) const;
  Vec2 separationOf (int ch,
                     std::array<ShipOrders, flightShips> const &orders) const;
  void redrawWanderOnANewBar (double beats, int beatsPerBar);

  FlightTuning _tuning;
  std::array<ShipState, flightShips> _ships{};
  std::array<float, flightShips> _phaseOffset{}; // set by launch: rabbit starts at the nearest phase
  std::array<Vec2, flightShips> _wander{};
  std::array<EscortLeg, flightShips> _escort{};
  std::array<juce::Random, flightShips> _dice;
  long long _lastWanderBar = -1;
  bool _breathing = false;
};

}
