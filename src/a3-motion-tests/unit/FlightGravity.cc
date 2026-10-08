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
#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/ShipDynamics.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

// The reason for this phase: the guest groups on the floor, acting as
// planets, must make the flight worth listening to. These tests fly one ship
// round the big path with the groups' gravity on and pin what a guest should
// hear: it bends towards a group, whips past a hotspot faster, keeps clear of
// a dead zone, is never the same lap twice, and never breaks.

using namespace a3;

namespace
{

constexpr int fourFour = 4;
constexpr float roomSlack = 1e-5f;
constexpr float twoPi = 2.f * pi<float> ();

// A speed-up has to reach 1.5x to be heard as "faster" at all (Carlile & Best;
// see .claude/notes/auditory-motion-research.md, B1), so that is the bar a
// slingshot has to clear, not the plan's 1.2x.
constexpr float audibleSpeedUp = 1.5f;

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
beatsIn (float bars)
{
  return static_cast<double> (bars) * fourFour;
}

struct Sample
{
  double beats; // when, after the step
  ShipState ship;
};

FlightBodies
bodiesOf (std::initializer_list<FlightBody> list)
{
  FlightBodies bodies;
  for (auto const &body : list)
    bodies.body[static_cast<size_t> (bodies.count++)] = body;
  return bodies;
}

/** Ship 0 for `bars`, chasing its rabbit with the bodies' gravity at a fixed
 *  `pulse`, from the rabbit's own place and pace. One sample a tick. */
std::vector<Sample>
fly (float bars, FlightBodies const &bodies, float pulse = 1.f)
{
  FlightTuning const tuning;
  auto const dt = tickBeats ();
  auto const start = rabbitAt (0., 0, fourFour, tuning);
  ShipState ship{ start.at, start.velocity };
  std::vector<Sample> path;
  for (size_t i = 0; i < ticksIn (beatsIn (bars)); ++i)
    {
      auto const now = static_cast<double> (i) * dt;
      ShipForces const forces{ rabbitAt (now, 0, fourFour, tuning),
                               gravityAt (ship.p, bodies, pulse, tuning),
                               {} };
      ship = stepShip (ship, forces, dt, tuning);
      path.push_back ({ now + dt, ship });
    }
  return path;
}

Vec2
rabbitOf (double beats)
{
  return rabbitAt (beats, 0, fourFour, FlightTuning{}).at;
}

float
closestApproach (std::vector<Sample> const &path, Vec2 to)
{
  auto closest = path.front ().ship.p.getDistanceFrom (to);
  for (auto const &sample : path)
    closest = std::min (closest, sample.ship.p.getDistanceFrom (to));
  return closest;
}

size_t
closestIndex (std::vector<Sample> const &path, Vec2 to)
{
  auto const nearer = [to] (Sample const &a, Sample const &b) {
    return a.ship.p.getDistanceFrom (to) < b.ship.p.getDistanceFrom (to);
  };
  return static_cast<size_t> (
      std::min_element (path.begin (), path.end (), nearer) - path.begin ());
}

Vec2
rotated (Vec2 v, float angle)
{
  auto const c = std::cos (angle);
  auto const s = std::sin (angle);
  return { c * v.x - s * v.y, s * v.x + c * v.y };
}

/** `p` in the frame that turns with the big path, so two laps can be laid on
 *  top of each other: the ellipse itself precesses an eighth of a turn a lap,
 *  which alone would make every lap "different". */
Vec2
inThePathsFrame (Sample const &sample)
{
  FlightTuning const tuning;
  auto const period = static_cast<double> (tuning.orbitPrecessionBars) * fourFour;
  auto const turns = sample.beats / period - std::floor (sample.beats / period);
  return rotated (sample.ship.p, -twoPi * static_cast<float> (turns));
}

/** The largest gap between lap 2 and lap 3 at the same lap phase. */
float
lapTwoAgainstLapThree (std::vector<Sample> const &path)
{
  FlightTuning const tuning;
  auto const lap = ticksIn (beatsIn (tuning.orbitLapBars));
  auto widest = 0.f;
  for (size_t i = 0; i < lap; ++i)
    widest = std::max (widest, inThePathsFrame (path[lap + i])
                                   .getDistanceFrom (inThePathsFrame (path[2 * lap + i])));
  return widest;
}

bool
isFinite (Vec2 v)
{
  return std::isfinite (v.x) && std::isfinite (v.y);
}

}

TEST (FlightGravity, AGroupBendsThePathTowardsIt)
{
  auto const angle = 0.25f * pi<float> ();
  Vec2 const group{ 0.45f * std::cos (angle), 0.45f * std::sin (angle) };
  auto const empty = closestApproach (fly (8.f, {}), group);
  auto const pulled = closestApproach (fly (8.f, bodiesOf ({ { group, 2.f } })), group);
  EXPECT_GE (empty - pulled, 0.1f) << "empty " << empty << ", pulled " << pulled;
}

TEST (FlightGravity, ACloseFlybySlingsTheShipOut)
{
  FlightTuning const tuning;
  auto const onThePath = rabbitOf (6.);
  auto const hotspot
      = onThePath + onThePath * (0.08f / onThePath.getDistanceFromOrigin ());
  auto const path = fly (8.f, bodiesOf ({ { hotspot, 3.f } }));

  auto const closest = closestIndex (path, hotspot);
  auto const before = ticksIn (0.5);
  auto const after = ticksIn (0.25);
  ASSERT_GE (closest, before);
  ASSERT_LT (closest + after, path.size ());

  auto const speedBefore = path[closest - before].ship.v.getDistanceFromOrigin ();
  auto const speedAfter = path[closest + after].ship.v.getDistanceFromOrigin ();
  EXPECT_GT (speedAfter, audibleSpeedUp * speedBefore)
      << "before " << speedBefore << ", after " << speedAfter;
  EXPECT_LE (speedAfter, tuning.speedMax + roomSlack);
}

TEST (FlightGravity, TheBigPathWinsFarFromTheGroups)
{
  Vec2 const group{ -0.8f, -0.8f };
  auto const towardsIt = group / group.getDistanceFromOrigin ();
  auto const path = fly (8.f, bodiesOf ({ { group, 2.f } }));
  auto worst = 0.f;
  for (auto const &sample : path)
    {
      auto const rabbit = rabbitOf (sample.beats);
      if (rabbit.getDotProduct (towardsIt) < 0.f) // the half away from it
        worst = std::max (worst, sample.ship.p.getDistanceFrom (rabbit));
    }
  EXPECT_LT (worst, 0.15f);
}

TEST (FlightGravity, ADeadZoneOnThePathIsFlownRound)
{
  FlightTuning const tuning;
  for (auto const placedAt : { 2., 6., 10., 13. })
    {
      auto const deadZone = rabbitOf (placedAt);
      auto const path = fly (8.f, bodiesOf ({ { deadZone, -2.f } }));
      EXPECT_GT (closestApproach (path, deadZone), tuning.captureRadius)
          << "placed where the rabbit is at beat " << placedAt;
    }
}

TEST (FlightGravity, TwoLapsAreNotTheSameWithAGroupThere)
{
  auto const empty = lapTwoAgainstLapThree (fly (16.f, {}));
  auto const withAGroup
      = lapTwoAgainstLapThree (fly (16.f, bodiesOf ({ { { 0.3f, 0.3f }, 1.f } })));
  EXPECT_LT (empty, 0.05f) << "the empty room repeats itself, as it should";
  EXPECT_GT (withAGroup, 0.05f);
}

TEST (FlightGravity, EightHeavyGroupsStayFinite)
{
  FlightTuning const tuning;
  auto const hardestPulse = 1.f + tuning.pulseDownbeatDepth;
  for (juce::int64 seed = 1; seed <= 20; ++seed)
    {
      juce::Random dice (spreadSeed (seed));
      FlightBodies bodies;
      for (auto &body : bodies.body)
        {
          auto const angle = twoPi * dice.nextFloat ();
          auto const radius = std::sqrt (dice.nextFloat ());
          body = { { radius * std::cos (angle), radius * std::sin (angle) },
                   dice.nextBool () ? 3.f : -3.f };
        }
      bodies.count = maxFlightBodies;

      for (auto const &sample : fly (32.f, bodies, hardestPulse))
        {
          ASSERT_TRUE (isFinite (sample.ship.p) && isFinite (sample.ship.v))
              << "seed " << seed << " at beat " << sample.beats;
          ASSERT_LE (sample.ship.p.getDistanceFromOrigin (), 1.f + roomSlack)
              << "seed " << seed << " at beat " << sample.beats;
          ASSERT_LE (sample.ship.v.getDistanceFromOrigin (),
                     tuning.speedMax + roomSlack)
              << "seed " << seed << " at beat " << sample.beats;
        }
    }
}
