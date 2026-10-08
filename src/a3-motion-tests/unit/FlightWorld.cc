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

#include <gtest/gtest.h>

#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-engine/flight/FlightWorld.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <cmath>
#include <cstring>
#include <vector>

using namespace a3;

namespace
{

constexpr int fourFour = 4;
constexpr juce::int64 aSeed = 7;
constexpr float speedSlack = 1e-5f;
constexpr float twoPi = 2.f * pi<float> ();

float
tickBeats ()
{
  return 1.f / static_cast<float> (TempoClock::getTicksPerBeat ());
}

size_t
ticksIn (double beats)
{
  return static_cast<size_t> (std::lround (beats * TempoClock::getTicksPerBeat ()));
}

double
barsToBeats (float bars)
{
  return static_cast<double> (bars) * fourFour;
}

std::array<ShipOrders, 4>
allPatrolling ()
{
  std::array<ShipOrders, 4> orders;
  for (auto &order : orders)
    order.flying = true;
  return orders;
}

/** The four ships launched on their own rabbits' places at beat 0. */
void
launchAll (FlightWorld &world)
{
  FlightTuning const tuning;
  for (auto ch = 0; ch < 4; ++ch)
    world.launch (ch, rabbitAt (0., ch, fourFour, tuning).at, 0., fourFour);
}

struct Sample
{
  double beats;
  std::array<ShipState, 4> ships;
};

/** `ticks` steps of `world` from beat `from`; one sample after each. */
std::vector<Sample>
run (FlightWorld &world, std::array<ShipOrders, 4> const &orders,
     FlightBodies const &bodies, size_t ticks, double from = 0.)
{
  auto const dt = tickBeats ();
  std::vector<Sample> path;
  for (size_t i = 0; i < ticks; ++i)
    {
      auto const now = from + static_cast<double> (i) * dt;
      world.step (orders, bodies, now, fourFour, 1.f, dt);
      Sample sample{ now + dt, {} };
      for (auto ch = 0; ch < 4; ++ch)
        sample.ships[static_cast<size_t> (ch)] = world.ship (ch);
      path.push_back (sample);
    }
  return path;
}

Vec2
rotated (Vec2 v, float angle)
{
  auto const c = std::cos (angle);
  auto const s = std::sin (angle);
  return { c * v.x - s * v.y, s * v.x + c * v.y };
}

/** `p` in the frame that turns with the big path (see FlightGravity.cc). */
Vec2
inThePathsFrame (Vec2 p, double beats)
{
  FlightTuning const tuning;
  auto const period = static_cast<double> (tuning.orbitPrecessionBars) * fourFour;
  auto const turns = beats / period - std::floor (beats / period);
  return rotated (p, -twoPi * static_cast<float> (turns));
}

}

TEST (FlightWorld, TwoEmptyLapsDifferByTheWander)
{
  FlightTuning const tuning;
  FlightWorld world (aSeed);
  launchAll (world);
  auto const path = run (world, allPatrolling (), {}, ticksIn (barsToBeats (12.f)));

  auto const lap = ticksIn (barsToBeats (tuning.orbitLapBars));
  auto widest = 0.f;
  for (size_t i = 0; i < lap; ++i)
    {
      auto const &two = path[lap + i];
      auto const &three = path[2 * lap + i];
      widest = std::max (widest,
                         inThePathsFrame (two.ships[0].p, two.beats)
                             .getDistanceFrom (inThePathsFrame (three.ships[0].p, three.beats)));
    }
  EXPECT_GT (widest, 0.02f);

  auto const nearest = tuning.orbitRadius * (1.f - tuning.orbitEccentricity) - 0.2f;
  auto const farthest = tuning.orbitRadius * (1.f + tuning.orbitEccentricity) + 0.2f;
  for (auto const &sample : path)
    for (auto const &ship : sample.ships)
      {
        auto const radius = ship.p.getDistanceFromOrigin ();
        ASSERT_GE (radius, nearest) << "at beat " << sample.beats;
        ASSERT_LE (radius, farthest) << "at beat " << sample.beats;
      }
}

TEST (FlightWorld, TwoShipsOnOneSpotDrawApart)
{
  FlightWorld world (aSeed);
  Vec2 const spot{ 0.5f, 0.2f };
  world.launch (0, spot, 0., fourFour);
  world.launch (1, spot, 0., fourFour);
  std::array<ShipOrders, 4> orders;
  orders[0].flying = true;
  orders[1].flying = true;
  ASSERT_EQ (world.ship (0).p, world.ship (1).p);

  run (world, orders, {}, ticksIn (barsToBeats (2.f)));
  EXPECT_GT (world.ship (0).p.getDistanceFrom (world.ship (1).p), 0.1f);
}

TEST (FlightWorld, SameSeedSameSky)
{
  FlightWorld first (aSeed), second (aSeed);
  launchAll (first);
  launchAll (second);
  auto const a = run (first, allPatrolling (), {}, 2000);
  auto const b = run (second, allPatrolling (), {}, 2000);
  ASSERT_EQ (a.size (), b.size ());
  EXPECT_EQ (std::memcmp (a.data (), b.data (), a.size () * sizeof (Sample)), 0);
}

TEST (FlightWorld, ADifferentSeedADifferentSky)
{
  FlightWorld first (aSeed), second (aSeed + 1);
  launchAll (first);
  launchAll (second);
  run (first, allPatrolling (), {}, 2000);
  run (second, allPatrolling (), {}, 2000);
  EXPECT_NE (first.ship (0).p, second.ship (0).p);
}

TEST (FlightWorld, AShipNotFlyingDoesNotMove)
{
  FlightWorld world (aSeed);
  launchAll (world);
  auto orders = allPatrolling ();
  orders[2].flying = false;
  auto const before = world.ship (2);
  run (world, orders, {}, 500);
  EXPECT_EQ (world.ship (2).p, before.p);
  EXPECT_EQ (world.ship (2).v, before.v);
}

TEST (FlightWorld, LaunchStartsWhereTheChannelIs)
{
  FlightTuning const tuning;
  FlightWorld world (aSeed);
  world.launch (0, { 0.3f, 0.1f }, 5.5, fourFour);
  EXPECT_EQ (world.ship (0).p, (Vec2{ 0.3f, 0.1f }));
  auto const speed = world.ship (0).v.getDistanceFromOrigin ();
  EXPECT_GE (speed, tuning.speedMin - speedSlack);
  EXPECT_LE (speed, tuning.speedMax + speedSlack);
}
