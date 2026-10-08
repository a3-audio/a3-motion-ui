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

#include <gtest/gtest.h>

#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-engine/flight/ShipDynamics.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

#include <cmath>
#include <cstring>
#include <vector>

using namespace a3;

namespace
{

constexpr int fourFour = 4;
constexpr float speedSlack = 1e-5f;

float
tickBeats ()
{
  return 1.f / static_cast<float> (TempoClock::getTicksPerBeat ());
}

int
ticksIn (double beats)
{
  return static_cast<int> (std::lround (beats * TempoClock::getTicksPerBeat ()));
}

bool
isFinite (Vec2 v)
{
  return std::isfinite (v.x) && std::isfinite (v.y);
}

struct Sample
{
  ShipState ship;
  Vec2 rabbit;
};

/** One ship chasing ship 0's rabbit in an empty room, one tick a step,
 *  starting at `start`; one sample after every step. */
std::vector<Sample>
flyEmpty (double beats, ShipState start = {})
{
  FlightTuning const tuning;
  auto const dt = tickBeats ();
  std::vector<Sample> path;
  auto ship = start;
  for (auto i = 0; i < ticksIn (beats); ++i)
    {
      auto const now = static_cast<double> (i) * dt;
      auto const rabbit = rabbitAt (now, 0, fourFour, tuning);
      ship = stepShip (ship, { rabbit, {}, {} }, dt, tuning);
      path.push_back ({ ship, rabbitAt (now + dt, 0, fourFour, tuning).at });
    }
  return path;
}

}

TEST (ShipDynamics, InAnEmptyRoomTheShipFollowsTheBigPath)
{
  auto const path = flyEmpty (12. * fourFour);
  auto const warmUp = static_cast<size_t> (ticksIn (4. * fourFour));
  auto worst = 0.f;
  for (auto i = warmUp; i < path.size (); ++i)
    worst = std::max (worst, path[i].ship.p.getDistanceFrom (path[i].rabbit));
  EXPECT_LT (worst, 0.1f);
}

TEST (ShipDynamics, ItNeverLeavesTheRoom)
{
  FlightTuning const tuning;
  auto const path = flyEmpty (2., { { 0.99f, 0.f }, { tuning.speedMax, 0.f } });
  for (auto const &sample : path)
    ASSERT_LE (sample.ship.p.getDistanceFromOrigin (), 1.f);
  auto const oneBeat = static_cast<size_t> (ticksIn (1.));
  EXPECT_LT (path[oneBeat - 1].ship.p.getDistanceFromOrigin (),
             tuning.rimSoft + 0.05f);
}

TEST (ShipDynamics, ItNeverStopsAndNeverRaces)
{
  FlightTuning const tuning;
  auto const dt = tickBeats ();
  ShipState ship{ { 0.2f, 0.1f }, { 3.f, -2.f } };
  for (auto i = 0; i < 1000; ++i)
    {
      // The goal sits on the ship and does not move: nothing to steer for.
      ship = stepShip (ship, { { ship.p, {} }, {}, {} }, dt, tuning);
      auto const speed = ship.v.getDistanceFromOrigin ();
      ASSERT_GE (speed, tuning.speedMin - speedSlack) << "step " << i;
      ASSERT_LE (speed, tuning.speedMax + speedSlack) << "step " << i;
    }
}

TEST (ShipDynamics, ADeadStartGetsGoing)
{
  FlightTuning const tuning;
  auto const after = stepShip ({}, { { {}, {} }, {}, {} }, tickBeats (), tuning);
  EXPECT_TRUE (isFinite (after.p) && isFinite (after.v));
  EXPECT_GE (after.v.getDistanceFromOrigin (), tuning.speedMin - speedSlack);
}

TEST (ShipDynamics, SameInputSameFlight)
{
  auto const first = flyEmpty (1000. * tickBeats ());
  auto const second = flyEmpty (1000. * tickBeats ());
  ASSERT_EQ (first.size (), 1000u);
  ASSERT_EQ (second.size (), first.size ());
  EXPECT_EQ (std::memcmp (first.data (), second.data (),
                          first.size () * sizeof (Sample)),
             0);
}
