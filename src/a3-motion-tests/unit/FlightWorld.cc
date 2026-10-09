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
#include <a3-motion-engine/flight/Breath.hh>
#include <a3-motion-engine/flight/FlightWorld.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
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

// Review Focus 2, the CLIP -> ORBIT half: the ship takes off where the
// channel is, never moves more than speedMax a tick, and chases a rabbit next
// to it rather than its channel's slot (here half a lap away).
TEST (FlightWorld, ALaunchedShipDoesNotJump)
{
  FlightTuning const tuning;
  auto const beats = 9.3;
  auto const launchedAt = rabbitAt (beats, 2, fourFour, tuning).at;
  FlightWorld world (aSeed);
  world.launch (0, launchedAt, beats, fourFour);
  std::array<ShipOrders, 4> orders;
  orders[0].flying = true;

  auto const path = run (world, orders, {}, ticksIn (barsToBeats (2.f)), beats);
  auto const widestStep = tuning.speedMax * tickBeats () + speedSlack;
  auto previous = launchedAt;
  auto farthestFromItsRabbit = 0.f;
  for (auto const &sample : path)
    {
      auto const p = sample.ships[0].p;
      ASSERT_LE (p.getDistanceFrom (previous), widestStep) << "beat " << sample.beats;
      previous = p;
      farthestFromItsRabbit = std::max (
          farthestFromItsRabbit,
          p.getDistanceFrom (rabbitAt (sample.beats, 2, fourFour, tuning).at));
    }
  // wander (<= wanderRadius) plus the steering's lag, nowhere near the
  // half lap (> 1) its own slot is away
  EXPECT_LE (farthestFromItsRabbit, 0.25f);
}

// ---- Escort: a group captures the ship (Task 7) ----

namespace
{

Vec2 const escorted{ 0.3f, -0.2f };
constexpr int escortedId = 5; // ids are the floor's, not indices

/** A crowd's mass, the default weight of the escort tests' group. */
float
crowd ()
{
  return FlightTuning{}.crowdMass;
}

FlightBodies
oneGroup (float mass = crowd ())
{
  FlightBodies bodies;
  bodies.body[0] = { escorted, mass, escortedId };
  bodies.count = 1;
  return bodies;
}

std::array<ShipOrders, 4>
shipZeroOn (FlightGoal goal, int bodyId)
{
  std::array<ShipOrders, 4> orders;
  orders[0] = { true, goal, bodyId };
  return orders;
}

/** Ship 0, launched on its rabbit, flown for `bars` under `orders`. */
std::vector<Sample>
flyShipZero (std::array<ShipOrders, 4> const &orders, FlightBodies const &bodies,
             float bars)
{
  FlightTuning const tuning;
  FlightWorld world (aSeed);
  world.launch (0, rabbitAt (0., 0, fourFour, tuning).at, 0., fourFour);
  return run (world, orders, bodies, ticksIn (barsToBeats (bars)));
}

float
escortRadius (float mass)
{
  return a3::escortRadius (mass, FlightTuning{});
}

/** The samples from bar 3 to bar 8: the ship has had time to be captured. */
std::vector<Sample>
captured (std::vector<Sample> const &path)
{
  return { path.begin () + static_cast<long> (ticksIn (barsToBeats (3.f))),
           path.begin () + static_cast<long> (ticksIn (barsToBeats (8.f))) };
}

float
widestFrom (std::vector<Sample> const &path, Vec2 centre)
{
  auto widest = 0.f;
  for (auto const &sample : path)
    widest = std::max (widest, sample.ships[0].p.getDistanceFrom (centre));
  return widest;
}

bool
sameFlight (std::vector<Sample> const &a, std::vector<Sample> const &b)
{
  return a.size () == b.size ()
         && std::memcmp (a.data (), b.data (), a.size () * sizeof (Sample)) == 0;
}

}

TEST (FlightWorld, AnEscortedGroupCapturesTheShip)
{
  auto const path = captured (
      flyShipZero (shipZeroOn (FlightGoal::Escort, escortedId), oneGroup (), 8.f));
  auto const radius = escortRadius (crowd ());
  for (auto const &sample : path)
    {
      auto const distance = sample.ships[0].p.getDistanceFrom (escorted);
      ASSERT_GE (distance, 0.5f * radius) << "at beat " << sample.beats;
      ASSERT_LE (distance, 1.6f * radius) << "at beat " << sample.beats;
    }
}

TEST (FlightWorld, ACapturedShipCirclesIt)
{
  auto const path = captured (
      flyShipZero (shipZeroOn (FlightGoal::Escort, escortedId), oneGroup (), 8.f));

  auto angleOf = [] (Sample const &s) {
    auto const d = s.ships[0].p - escorted;
    return std::atan2 (d.y, d.x);
  };
  // Unwrap the angle round the group, then judge it in the direction it
  // mostly turns: one whole turn at least, and never a swing back.
  std::vector<float> turned{ 0.f };
  for (size_t i = 1; i < path.size (); ++i)
    turned.push_back (turned.back ()
                      + std::remainder (angleOf (path[i]) - angleOf (path[i - 1]),
                                        twoPi));
  auto const direction = turned.back () >= 0.f ? 1.f : -1.f;
  EXPECT_GE (direction * turned.back (), twoPi);

  auto furthest = 0.f;
  auto worstReversal = 0.f;
  for (auto const angle : turned)
    {
      furthest = std::max (furthest, direction * angle);
      worstReversal = std::max (worstReversal, furthest - direction * angle);
    }
  EXPECT_LE (worstReversal, 0.3f);
}

// An escort has to be heard as a circle, not as a source parked over the
// group: seen from the room's centre it must sweep >= 30 deg (research B2).
// Near the rim that is where it fails if the escorted group's own pull is
// added on top of the circle: the part of the circle outside the room is cut
// off, the rim pushes the ship inwards, and the group swallows it (measured
// with the plan's masses 1 and 2: 10 and 13 deg with the group's own pull, 58
// and 81 deg without it). A light G's circle is smaller, and must pass too.
TEST (FlightWorld, AnEscortNearTheRimIsHeardAsACircle)
{
  constexpr float audibleSweep = 30.f * pi<float> () / 180.f;
  FlightTuning const t;
  for (auto const &group : { FlightBody{ { 0.75f, 0.f }, t.groupMass, escortedId },
                             FlightBody{ { 0.6f, -0.5f }, t.groupMass, escortedId },
                             FlightBody{ { 0.6f, -0.5f }, t.crowdMass, escortedId } })
    {
      FlightBodies bodies;
      bodies.body[0] = group;
      bodies.count = 1;
      auto const path = captured (
          flyShipZero (shipZeroOn (FlightGoal::Escort, escortedId), bodies, 8.f));

      auto const facing = std::atan2 (group.at.y, group.at.x);
      auto narrowest = 0.f, widest = 0.f;
      for (auto const &sample : path)
        {
          auto const p = sample.ships[0].p;
          auto const off = std::remainder (std::atan2 (p.y, p.x) - facing, twoPi);
          narrowest = std::min (narrowest, off);
          widest = std::max (widest, off);
        }
      EXPECT_GE (widest - narrowest, audibleSweep)
          << "a group of mass " << group.mass << " at " << group.at.x << ", "
          << group.at.y;
    }
}

TEST (FlightWorld, OtherGroupsStillTug)
{
  auto alone = oneGroup ();
  auto withANeighbour = oneGroup ();
  // beyond the escort circle, between it and the room's centre
  auto const towardsTheCentre = -escorted / escorted.getDistanceFromOrigin ();
  withANeighbour.body[1]
      = { escorted + towardsTheCentre * (1.5f * escortRadius (crowd ())), crowd (),
          escortedId + 1 };
  withANeighbour.count = 2;

  auto const orders = shipZeroOn (FlightGoal::Escort, escortedId);
  auto const quiet = widestFrom (captured (flyShipZero (orders, alone, 8.f)), escorted);
  auto const tugged
      = widestFrom (captured (flyShipZero (orders, withANeighbour, 8.f)), escorted);
  EXPECT_GT (tugged, quiet);
}

TEST (FlightWorld, EscortingADeadZoneIsPatrol)
{
  auto const deadZone = oneGroup (FlightTuning{}.deadZoneMass);
  EXPECT_TRUE (sameFlight (
      flyShipZero (shipZeroOn (FlightGoal::Escort, escortedId), deadZone, 4.f),
      flyShipZero (shipZeroOn (FlightGoal::Patrol, noBodyId), deadZone, 4.f)));
}

TEST (FlightWorld, EscortingAMissingBodyIsPatrol)
{
  auto const bodies = oneGroup ();
  EXPECT_TRUE (sameFlight (
      flyShipZero (shipZeroOn (FlightGoal::Escort, escortedId + 1), bodies, 4.f),
      flyShipZero (shipZeroOn (FlightGoal::Patrol, noBodyId), bodies, 4.f)));
  EXPECT_TRUE (sameFlight (
      flyShipZero (shipZeroOn (FlightGoal::Escort, noBodyId), bodies, 4.f),
      flyShipZero (shipZeroOn (FlightGoal::Patrol, noBodyId), bodies, 4.f)));
}

TEST (FlightWorld, ABodyRemovedMidEscortLetsTheShipGo)
{
  FlightTuning const tuning;
  FlightWorld world (aSeed);
  world.launch (0, rabbitAt (0., 0, fourFour, tuning).at, 0., fourFour);
  auto const orders = shipZeroOn (FlightGoal::Escort, escortedId);
  auto const removedAt = barsToBeats (4.f);
  run (world, orders, oneGroup (), ticksIn (removedAt));
  ASSERT_LE (world.ship (0).p.getDistanceFrom (escorted), 1.6f * escortRadius (crowd ()));

  auto const after = run (world, orders, FlightBodies{},
                          ticksIn (barsToBeats (4.f)), removedAt);
  for (auto const &sample : after)
    ASSERT_TRUE (std::isfinite (sample.ships[0].p.x) && std::isfinite (sample.ships[0].p.y)
                 && std::isfinite (sample.ships[0].v.x) && std::isfinite (sample.ships[0].v.y));
  auto const &last = after.back ();
  EXPECT_LE (last.ships[0].p.getDistanceFrom (
                 rabbitAt (last.beats, 0, fourFour, tuning).at),
             0.15f);
}

// The floor removes bodies (a long press), and the ones after it move down
// an index. An escort is keyed by the body's id, so the ship stays with its
// group and its circle goes on where it was: no new leg, no jump.
TEST (FlightWorld, AnEscortStaysWithItsGroupWhenAnEarlierOneIsRemoved)
{
  FlightBodies both;
  both.body[0] = { { -0.5f, 0.4f }, 1.f, escortedId + 1 };
  both.body[1] = { escorted, crowd (), escortedId };
  both.count = 2;
  FlightBodies justIt;
  justIt.body[0] = both.body[1];
  justIt.count = 1;

  FlightTuning const tuning;
  FlightWorld world (aSeed);
  world.launch (0, rabbitAt (0., 0, fourFour, tuning).at, 0., fourFour);
  auto const orders = shipZeroOn (FlightGoal::Escort, escortedId);
  auto const removedAt = barsToBeats (4.f);
  auto path = run (world, orders, both, ticksIn (removedAt));
  path.erase (path.begin (), path.end () - static_cast<long> (ticksIn (barsToBeats (1.f))));
  auto const after = run (world, orders, justIt, ticksIn (barsToBeats (4.f)), removedAt);
  path.insert (path.end (), after.begin (), after.end ());

  auto const radius = escortRadius (crowd ());
  auto angleOf = [] (Sample const &s) {
    auto const d = s.ships[0].p - escorted;
    return std::atan2 (d.y, d.x);
  };
  auto const direction
      = std::remainder (angleOf (path[1]) - angleOf (path[0]), twoPi) >= 0.f ? 1.f : -1.f;
  auto turned = 0.f, furthest = 0.f, worstReversal = 0.f;
  for (size_t i = 1; i < path.size (); ++i)
    {
      auto const distance = path[i].ships[0].p.getDistanceFrom (escorted);
      ASSERT_GE (distance, 0.5f * radius) << "at beat " << path[i].beats;
      ASSERT_LE (distance, 1.6f * radius) << "at beat " << path[i].beats;
      turned += direction
                * std::remainder (angleOf (path[i]) - angleOf (path[i - 1]), twoPi);
      furthest = std::max (furthest, turned);
      worstReversal = std::max (worstReversal, furthest - turned);
    }
  EXPECT_LE (worstReversal, 0.3f);
}

// A capture lap of 0 bars (or less) leaves the escort goal standing at the
// side the ship came in from: still finite, still next to the group.
TEST (FlightWorld, AnEscortWithoutACaptureLapStandsBesideItsGroup)
{
  for (auto const lapBars : { 0.f, -2.f })
    {
      FlightTuning tuning;
      tuning.captureLapBars = lapBars;
      FlightWorld world (aSeed, tuning);
      world.launch (0, rabbitAt (0., 0, fourFour, tuning).at, 0., fourFour);
      for (auto const &sample : run (world, shipZeroOn (FlightGoal::Escort, escortedId),
                                     oneGroup (), ticksIn (barsToBeats (4.f))))
        {
          auto const &ship = sample.ships[0];
          ASSERT_TRUE (std::isfinite (ship.p.x) && std::isfinite (ship.p.y)
                       && std::isfinite (ship.v.x) && std::isfinite (ship.v.y))
              << "lap " << lapBars << " at beat " << sample.beats;
          ASSERT_LE (ship.p.getDistanceFromOrigin (), 1.f + speedSlack);
          ASSERT_LE (ship.v.getDistanceFromOrigin (), tuning.speedMax + speedSlack);
          if (sample.beats >= barsToBeats (2.f))
            ASSERT_LE (ship.p.getDistanceFrom (escorted), 1.6f * escortRadius (crowd ()))
                << "lap " << lapBars << " at beat " << sample.beats;
        }
    }
}


// ---- Ships among themselves, and the worst a floor can do ----

// The ships keep apart by their own softening: tuning how sharp a group's
// well is must not change how close two ships may come.
TEST (FlightWorld, TheShipsKeepApartWhateverTheGravitysSoftening)
{
  FlightTuning sharp, soft;
  sharp.softening = 0.04f;
  soft.softening = 0.3f;
  FlightWorld first (aSeed, sharp), second (aSeed, soft);
  launchAll (first);
  launchAll (second);
  Vec2 const spot{ 0.5f, 0.2f };
  first.launch (1, spot, 0., fourFour);
  first.launch (2, spot, 0., fourFour);
  second.launch (1, spot, 0., fourFour);
  second.launch (2, spot, 0., fourFour);
  EXPECT_TRUE (sameFlight (run (first, allPatrolling (), {}, 2000),
                           run (second, allPatrolling (), {}, 2000)));
}

namespace
{

/** Eight bodies thrown on the floor: ids 1..8, a mix of groups, crowds,
 *  hotspots and dead zones, anywhere in the room. */
FlightBodies
aCrowdedFloor (juce::Random &dice)
{
  FlightTuning const t;
  float const masses[] = { t.groupMass, t.crowdMass, t.hotspotMass, t.deadZoneMass };
  FlightBodies bodies;
  for (auto i = 0; i < maxFlightBodies; ++i)
    {
      auto const angle = twoPi * dice.nextFloat ();
      auto const radius = std::sqrt (dice.nextFloat ());
      bodies.body[static_cast<size_t> (i)]
          = { { radius * std::cos (angle), radius * std::sin (angle) },
              masses[dice.nextInt (4)], i + 1 };
    }
  bodies.count = maxFlightBodies;
  return bodies;
}

/** `bodies` without its body at `index`, the later ones moved down: what
 *  the floor does on a long press. */
FlightBodies
withoutIndex (FlightBodies const &bodies, int index)
{
  FlightBodies left;
  for (auto i = 0; i < bodies.count; ++i)
    if (i != index)
      left.body[static_cast<size_t> (left.count++)]
          = bodies.body[static_cast<size_t> (i)];
  return left;
}

}

// Review Focus 3: never NaN, never out of the room, never faster than
// speedMax, with all four ships up, two of them escorting (one of them a
// body that is removed half way, one perhaps a dead zone), eight bodies, the
// hardest pulse, 32 bars, twenty floors.
TEST (FlightWorld, FourShipsOnACrowdedFloorNeverBreak)
{
  FlightTuning const tuning;
  auto const hardestPulse = 1.f + tuning.pulseDownbeatDepth;
  auto const dt = tickBeats ();
  auto const half = ticksIn (barsToBeats (16.f));
  for (juce::int64 seed = 1; seed <= 20; ++seed)
    {
      juce::Random dice (spreadSeed (seed));
      auto bodies = aCrowdedFloor (dice);
      auto orders = allPatrolling ();
      orders[0] = { true, FlightGoal::Escort, 1 + dice.nextInt (maxFlightBodies) };
      orders[1] = { true, FlightGoal::Escort, 1 + dice.nextInt (maxFlightBodies) };
      auto const removed = 1 + dice.nextInt (maxFlightBodies - 1);

      FlightWorld world (seed, tuning);
      launchAll (world);
      for (size_t i = 0; i < 2 * half; ++i)
        {
          if (i == half)
            bodies = withoutIndex (bodies, removed - 1);
          auto const now = static_cast<double> (i) * dt;
          world.step (orders, bodies, now, fourFour, hardestPulse, dt);
          for (auto ch = 0; ch < 4; ++ch)
            {
              auto const &ship = world.ship (ch);
              ASSERT_TRUE (std::isfinite (ship.p.x) && std::isfinite (ship.p.y)
                           && std::isfinite (ship.v.x) && std::isfinite (ship.v.y))
                  << "seed " << seed << ", ship " << ch << ", beat " << now;
              ASSERT_LE (ship.p.getDistanceFromOrigin (), 1.f + speedSlack)
                  << "seed " << seed << ", ship " << ch << ", beat " << now;
              ASSERT_LE (ship.v.getDistanceFromOrigin (), tuning.speedMax + speedSlack)
                  << "seed " << seed << ", ship " << ch << ", beat " << now;
            }
        }
    }
}

// The breath (MJ lab, 2026-10-08, playbook rule 18): every flying ship stands
// still for the last beat of each bar and restarts on the one.

TEST (FlightBreath, HoldsOnTheLastBeatOfTheBar)
{
  EXPECT_FALSE (breathHolds (0., fourFour));
  EXPECT_FALSE (breathHolds (2.99, fourFour));
  EXPECT_TRUE (breathHolds (3., fourFour));
  EXPECT_TRUE (breathHolds (3.99, fourFour));
  EXPECT_FALSE (breathHolds (4., fourFour));
  EXPECT_TRUE (breathHolds (7.5, fourFour));
  EXPECT_TRUE (breathHolds (2.5, 3));
  EXPECT_FALSE (breathHolds (1.5, 3));
}

TEST (FlightBreath, NoBarNoBreath)
{
  EXPECT_FALSE (breathHolds (3.5, 0));
  EXPECT_FALSE (breathHolds (-0.5, fourFour));
}

TEST (FlightBreath, IsOffUntilAskedFor)
{
  FlightWorld world (aSeed);
  EXPECT_FALSE (world.breathing ());
  launchAll (world);
  auto const before = world.ship (0).p;
  run (world, allPatrolling (), {}, ticksIn (1.), 3.);
  EXPECT_NE (world.ship (0).p, before) << "without the breath beat 4 flies";
}

TEST (FlightBreath, EveryShipStandsStillOnBeatFour)
{
  FlightWorld world (aSeed);
  world.setBreathing (true);
  launchAll (world);
  run (world, allPatrolling (), oneGroup (), ticksIn (3.));
  std::array<ShipState, 4> before;
  for (auto ch = 0; ch < 4; ++ch)
    before[static_cast<size_t> (ch)] = world.ship (ch);

  run (world, allPatrolling (), oneGroup (), ticksIn (1.), 3.);
  for (auto ch = 0; ch < 4; ++ch)
    {
      EXPECT_EQ (world.ship (ch).p, before[static_cast<size_t> (ch)].p) << "ship " << ch;
      EXPECT_EQ (world.ship (ch).v, before[static_cast<size_t> (ch)].v) << "ship " << ch;
    }
}

TEST (FlightBreath, RestartsOnTheOne)
{
  FlightWorld world (aSeed);
  world.setBreathing (true);
  launchAll (world);
  run (world, allPatrolling (), {}, ticksIn (4.));
  auto const held = world.ship (0).p;
  run (world, allPatrolling (), {}, 1, 4.);
  EXPECT_NE (world.ship (0).p, held);
}

TEST (FlightBreath, TheShipCatchesUpWithItsRabbit)
{
  // The rabbit runs on through the stop, so the restart is a short chase; by
  // the third beat of the next bar the ship is back where an unbroken flight
  // would be.
  FlightWorld breathing (aSeed), steady (aSeed);
  breathing.setBreathing (true);
  launchAll (breathing);
  launchAll (steady);
  auto const until = barsToBeats (4.f) + 3.;
  run (breathing, allPatrolling (), {}, ticksIn (until));
  run (steady, allPatrolling (), {}, ticksIn (until));
  for (auto ch = 0; ch < 4; ++ch)
    EXPECT_LT ((breathing.ship (ch).p - steady.ship (ch).p).getDistanceFromOrigin (),
               0.1f)
        << "ship " << ch;
}

// Under three beats a bar the last beat is half the bar or all of it: that
// is a stutter or a standstill, not a breath, so there is none.
TEST (FlightBreath, NoBreathUnderThreeBeatsABar)
{
  EXPECT_FALSE (breathHolds (0.5, 1));
  EXPECT_FALSE (breathHolds (1.5, 2));
  EXPECT_TRUE (breathHolds (2.5, 3));
}

// The key is tapped whenever the DJ likes, but a stop is a whole beat on the
// grid or it is not a breath: a switch made inside beat 4 waits for the one.
TEST (FlightBreath, SwitchedOnInsideBeatFourItWaitsForTheOne)
{
  FlightWorld world (aSeed);
  launchAll (world);
  run (world, allPatrolling (), {}, ticksIn (3.5));
  world.setBreathing (true);
  EXPECT_TRUE (world.breathing ()) << "the key says what was asked for";

  auto const before = world.ship (0).p;
  run (world, allPatrolling (), {}, ticksIn (0.5), 3.5);
  EXPECT_NE (world.ship (0).p, before) << "no stop cut short";

  run (world, allPatrolling (), {}, ticksIn (3.), 4.);
  auto const held = world.ship (0).p;
  run (world, allPatrolling (), {}, ticksIn (1.), 7.);
  EXPECT_EQ (world.ship (0).p, held) << "the next bar's beat 4 is a stop";
}

TEST (FlightBreath, SwitchedOffInsideBeatFourItHoldsToTheOne)
{
  FlightWorld world (aSeed);
  world.setBreathing (true);
  launchAll (world);
  run (world, allPatrolling (), {}, ticksIn (3.5));
  auto const held = world.ship (0).p;
  world.setBreathing (false);
  EXPECT_FALSE (world.breathing ());

  run (world, allPatrolling (), {}, ticksIn (0.5), 3.5);
  EXPECT_EQ (world.ship (0).p, held) << "the stop runs to the one";
  run (world, allPatrolling (), {}, 1, 4.);
  EXPECT_NE (world.ship (0).p, held);

  run (world, allPatrolling (), {}, ticksIn (3.), 4. + tickBeats ());
  auto const before = world.ship (0).p;
  run (world, allPatrolling (), {}, ticksIn (1.), 7. + tickBeats ());
  EXPECT_NE (world.ship (0).p, before) << "off means beat 4 flies again";
}

// A ship put into the air inside a stop waits there for the one, like the
// others, rather than starting a bar of its own.
TEST (FlightBreath, ALaunchInsideAStopWaitsForTheOne)
{
  FlightTuning const tuning;
  FlightWorld world (aSeed);
  world.setBreathing (true);
  run (world, allPatrolling (), {}, ticksIn (3.5));
  auto const at = rabbitAt (3.5, 0, fourFour, tuning).at;
  world.launch (0, at, 3.5, fourFour);

  run (world, allPatrolling (), {}, ticksIn (0.5), 3.5);
  EXPECT_EQ (world.ship (0).p, at);
  run (world, allPatrolling (), {}, 1, 4.);
  EXPECT_NE (world.ship (0).p, at);
}

// An escort keeps its group through the stops: captured, and circling it,
// with the breath on as without.
TEST (FlightBreath, AnEscortKeepsItsGroupAcrossTheStops)
{
  FlightTuning const tuning;
  FlightWorld world (aSeed);
  world.setBreathing (true);
  world.launch (0, rabbitAt (0., 0, fourFour, tuning).at, 0., fourFour);
  auto const path = run (world, shipZeroOn (FlightGoal::Escort, escortedId),
                         oneGroup (), ticksIn (barsToBeats (8.f)));

  auto const radius = escortRadius (crowd ());
  for (auto const &sample : captured (path))
    {
      auto const distance = sample.ships[0].p.getDistanceFrom (escorted);
      ASSERT_GE (distance, 0.5f * radius) << "at beat " << sample.beats;
      ASSERT_LE (distance, 1.6f * radius) << "at beat " << sample.beats;
    }

  // Standing still on every beat 4 of the captured stretch.
  for (size_t i = 1; i < path.size (); ++i)
    if (breathHolds (path[i].beats - tickBeats (), fourFour))
      ASSERT_EQ (path[i].ships[0].p, path[i - 1].ships[0].p)
          << "at beat " << path[i].beats;
}

// -- An action on a flying ship ------------------------------------

namespace
{
std::array<ShipOrders, 4>
shipZeroDrivenBy (FlightMotion const &motion, bool driven = true)
{
  auto orders = allPatrolling ();
  orders[0].motion = motion;
  orders[0].driven = driven;
  return orders;
}

/** How far ship `ch` went round the room's middle along `path`, in degrees,
 *  counter-clockwise positive. */
double
turnedRound (std::vector<Sample> const &path, int ch)
{
  auto turned = 0.;
  for (size_t i = 1; i < path.size (); ++i)
    {
      auto const a = path[i - 1].ships[static_cast<size_t> (ch)].p;
      auto const b = path[i].ships[static_cast<size_t> (ch)].p;
      turned += std::remainder (std::atan2 (static_cast<double> (b.y), static_cast<double> (b.x))
                                    - std::atan2 (static_cast<double> (a.y), static_cast<double> (a.x)),
                                2. * pi<double> ());
    }
  return turned * 180. / pi<double> ();
}

double
meanRadius (std::vector<Sample> const &path, int ch, size_t from, size_t to)
{
  auto sum = 0.;
  for (auto i = from; i < to; ++i)
    sum += static_cast<double> (path[i].ships[static_cast<size_t> (ch)].p.getDistanceFromOrigin ());
  return sum / static_cast<double> (to - from);
}

FlightMotion
spinOf (int step)
{
  FlightMotion motion;
  motion.spin = step;
  return motion;
}
}

TEST (FlightMotionWorld, AnUndrivenMotionChangesNothing)
{
  FlightWorld plain (aSeed);
  FlightWorld waiting (aSeed);
  launchAll (plain);
  launchAll (waiting);
  auto const a = run (plain, allPatrolling (), oneGroup (), ticksIn (8.));
  auto const b = run (waiting, shipZeroDrivenBy (spinOf (8), false), oneGroup (), ticksIn (8.));
  for (size_t i = 0; i < a.size (); ++i)
    for (auto ch = 0; ch < 4; ++ch)
      ASSERT_EQ (a[i].ships[static_cast<size_t> (ch)].p, b[i].ships[static_cast<size_t> (ch)].p)
          << "tick " << i << ", ship " << ch;
}

TEST (FlightMotionWorld, TheActionTakesHoldOverOneBeatAndLetsGoOverOne)
{
  FlightWorld world (aSeed);
  launchAll (world);
  run (world, shipZeroDrivenBy (spinOf (6)), {}, ticksIn (0.5));
  EXPECT_NEAR (world.motionWeight (0), 0.5f, 2.f * tickBeats ());
  EXPECT_FLOAT_EQ (world.motionWeight (1), 0.f) << "only the ship it was fired at";
  run (world, shipZeroDrivenBy (spinOf (6)), {}, ticksIn (1.), 0.5);
  EXPECT_FLOAT_EQ (world.motionWeight (0), 1.f);
  run (world, shipZeroDrivenBy ({}, false), {}, ticksIn (0.5), 1.5);
  EXPECT_NEAR (world.motionWeight (0), 0.5f, 2.f * tickBeats ());
  EXPECT_TRUE (world.motion (0).spin.has_value ()) << "kept while it lets go";
  run (world, shipZeroDrivenBy ({}, false), {}, ticksIn (1.), 2.);
  EXPECT_FLOAT_EQ (world.motionWeight (0), 0.f);
  EXPECT_FALSE (world.motion (0).any ());
}

// A lap a bar against the path's four: the other way round (a positive spin
// turns as a clip's does) and four times as far.
TEST (FlightMotionWorld, ASpinCarriesAPatrollingShipRound)
{
  FlightWorld plain (aSeed);
  FlightWorld spun (aSeed);
  launchAll (plain);
  launchAll (spun);
  auto const free = turnedRound (run (plain, allPatrolling (), {}, ticksIn (16.)), 0);
  auto const carried = turnedRound (run (spun, shipZeroDrivenBy (spinOf (6)), {}, ticksIn (16.)), 0);
  EXPECT_GT (free, 250.) << "the path's own lap";
  EXPECT_LT (carried, -1100.) << "four laps the other way, less the beat it took to take hold";
}

TEST (FlightMotionWorld, APaceDoublesTheLap)
{
  FlightWorld plain (aSeed);
  FlightWorld paced (aSeed);
  launchAll (plain);
  launchAll (paced);
  FlightMotion faster;
  faster.speedLog2 = -1;
  auto const free = turnedRound (run (plain, allPatrolling (), {}, ticksIn (16.)), 0);
  auto const quick = turnedRound (run (paced, shipZeroDrivenBy (faster), {}, ticksIn (16.)), 0);
  EXPECT_GT (quick / free, 1.7);
  EXPECT_LT (quick / free, 2.3);
}

TEST (FlightMotionWorld, ASwellBreathesThePathIn)
{
  FlightWorld plain (aSeed);
  FlightWorld breathing (aSeed);
  launchAll (plain);
  launchAll (breathing);
  FlightMotion in;
  in.swell = -5; // a breath in two bars
  auto const a = run (plain, allPatrolling (), {}, ticksIn (8.));
  auto const b = run (breathing, shipZeroDrivenBy (in), {}, ticksIn (8.));
  // Around the breath's deepest point, beat 4.
  EXPECT_LT (meanRadius (b, 0, ticksIn (3.), ticksIn (6.)),
             0.75 * meanRadius (a, 0, ticksIn (3.), ticksIn (6.)));
}

// An escort's order is its circle.
TEST (FlightMotionWorld, AnEscortIgnoresTheFloorKeys)
{
  FlightWorld plain (aSeed);
  FlightWorld driven (aSeed);
  launchAll (plain);
  launchAll (driven);
  auto escorting = allPatrolling ();
  escorting[0].goal = FlightGoal::Escort;
  escorting[0].bodyId = oneGroup ().body[0].id;
  auto drivenEscort = escorting;
  FlightMotion everything = spinOf (8);
  everything.swell = 8;
  everything.speedLog2 = -7;
  drivenEscort[0].motion = everything;
  drivenEscort[0].driven = true;
  auto const a = run (plain, escorting, oneGroup (), ticksIn (8.));
  auto const b = run (driven, drivenEscort, oneGroup (), ticksIn (8.));
  for (size_t i = 0; i < a.size (); ++i)
    ASSERT_EQ (a[i].ships[0].p, b[i].ships[0].p) << "tick " << i;
}

// The breath holds the floor, an action's carry included.
TEST (FlightMotionWorld, TheBreathHoldsADrivenShip)
{
  FlightWorld world (aSeed);
  world.setBreathing (true);
  launchAll (world);
  run (world, shipZeroDrivenBy (spinOf (7)), {}, ticksIn (3.));
  auto const before = world.ship (0);
  run (world, shipZeroDrivenBy (spinOf (7)), {}, ticksIn (1.), 3.);
  EXPECT_EQ (world.ship (0).p, before.p);
}

TEST (FlightMotionWorld, ALaunchStartsUndriven)
{
  FlightWorld world (aSeed);
  launchAll (world);
  run (world, shipZeroDrivenBy (spinOf (6)), {}, ticksIn (2.));
  ASSERT_FLOAT_EQ (world.motionWeight (0), 1.f);
  world.launch (0, world.ship (0).p, 2., fourFour);
  EXPECT_FLOAT_EQ (world.motionWeight (0), 0.f);
}

namespace
{
/** How far `p` is off the path as it stands at `beats`. */
float
offThePath (Vec2 p, double beats, FlightTuning const &tuning)
{
  auto const phase = nearestOrbitPhase (p, beats, fourFour, tuning);
  auto const onPath = rabbitAt (beats, 0, fourFour, tuning,
                                phase - rabbitSlotPhase (beats, 0, fourFour, tuning)).at;
  return (p - onPath).getDistanceFromOrigin ();
}

float
furthestOffThePath (std::vector<Sample> const &path, FlightTuning const &tuning)
{
  auto furthest = 0.f;
  for (auto const &sample : path)
    furthest = std::max (furthest, offThePath (sample.ships[0].p, sample.beats, tuning));
  return furthest;
}
}

// Carried, not steered. A ship alone on its path, without the
// wander, stays on the path under the fastest spin: the carry moves it
// along the ellipse with its rabbit, so the steering has nothing to undo.
TEST (FlightMotionWorld, ACarriedShipStaysOnThePath)
{
  FlightTuning tuning;
  tuning.wanderRadius = 0.f;
  std::array<ShipOrders, 4> alone{};
  alone[0].flying = true;
  auto driven = alone;
  driven[0].motion = spinOf (8);
  driven[0].driven = true;

  FlightWorld plain (aSeed, tuning);
  FlightWorld spun (aSeed, tuning);
  plain.launch (0, rabbitAt (0., 0, fourFour, tuning).at, 0., fourFour);
  spun.launch (0, rabbitAt (0., 0, fourFour, tuning).at, 0., fourFour);
  auto const free = furthestOffThePath (run (plain, alone, {}, ticksIn (16.)), tuning);
  auto const carried = furthestOffThePath (run (spun, driven, {}, ticksIn (16.)), tuning);
  EXPECT_LT (carried, free + 0.01f) << "free " << free;
}

// -- A game's steering goal -----------------------------------------------------
// A game never moves a ship itself: it hands the world a point, and the world
// flies to it under the same steering, gravity and walls as everything else.

namespace
{
std::array<ShipOrders, 4>
shipZeroSteeringFor (OrbitPoint goal)
{
  auto orders = allPatrolling ();
  orders[0].goal = FlightGoal::Steer;
  orders[0].steer = goal;
  return orders;
}
}

TEST (FlightSteer, AShipFliesToThePointItIsGiven)
{
  FlightWorld world (aSeed);
  launchAll (world);
  Vec2 const point{ -0.5f, 0.3f };
  run (world, shipZeroSteeringFor ({ point, {} }), {}, ticksIn (barsToBeats (2.f)));
  EXPECT_LT (world.ship (0).p.getDistanceFrom (point), 0.15f);
}

TEST (FlightSteer, ASteeringShipIsNotCarriedByAnAction)
{
  FlightWorld plain (aSeed), driven (aSeed);
  launchAll (plain);
  launchAll (driven);
  OrbitPoint const goal{ { 0.2f, -0.4f }, {} };
  auto withSpin = shipZeroSteeringFor (goal);
  withSpin[0].motion.spin = 8;
  withSpin[0].driven = true;
  run (plain, shipZeroSteeringFor (goal), {}, ticksIn (8.));
  run (driven, withSpin, {}, ticksIn (8.));
  EXPECT_EQ (plain.ship (0).p, driven.ship (0).p);
  EXPECT_EQ (plain.ship (0).v, driven.ship (0).v);
}

TEST (FlightSteer, ASteeringShipFliesThroughTheBreathsStop)
{
  FlightWorld world (aSeed);
  launchAll (world);
  world.setBreathing (true);
  auto const orders = shipZeroSteeringFor ({ { -0.6f, -0.2f }, {} });
  run (world, orders, {}, ticksIn (3.)); // up to beat 4, the stop
  auto const steererBefore = world.ship (0).p;
  auto const patrollerBefore = world.ship (1).p;
  run (world, orders, {}, ticksIn (0.5), 3.); // inside the stop
  EXPECT_NE (world.ship (0).p, steererBefore);
  EXPECT_EQ (world.ship (1).p, patrollerBefore);
}

TEST (FlightSteer, ASteeringShipIsStillPulledByTheGroups)
{
  FlightBodies bodies;
  bodies.body[0] = { { 0.f, 0.3f }, FlightTuning{}.hotspotMass, 0 };
  bodies.count = 1;
  FlightWorld alone (aSeed), pulled (aSeed);
  launchAll (alone);
  launchAll (pulled);
  auto const orders = shipZeroSteeringFor ({ { -0.7f, 0.f }, {} });
  run (alone, orders, {}, ticksIn (2.));
  run (pulled, orders, bodies, ticksIn (2.));
  EXPECT_NE (alone.ship (0).p, pulled.ship (0).p);
}

TEST (FlightSteer, ASteeringShipIsSparedThePullOfTheBodyItPlaysAgainst)
{
  // A game's figure already stands for its target's pull, as an escort's
  // circle does: named in the orders, that body no longer tugs at the ship,
  // while every other body still does. Ship 0 flies alone, so no other
  // ship's push tells the worlds apart.
  constexpr int targetId = 3;
  constexpr int otherId = 4;
  FlightBodies target;
  target.body[0] = { { -0.3f, 0.2f }, FlightTuning{}.groupMass, targetId };
  target.count = 1;
  auto both = target;
  both.body[1] = { { 0.4f, -0.4f }, FlightTuning{}.groupMass, otherId };
  both.count = 2;
  FlightBodies other;
  other.body[0] = both.body[1];
  other.count = 1;

  std::array<ShipOrders, 4> pulled{};
  pulled[0] = { true, FlightGoal::Steer, noBodyId };
  pulled[0].steer = { { -0.7f, 0.f }, {} };
  auto spared = pulled;
  spared[0].bodyId = targetId;
  auto const flownAlone = [] (std::array<ShipOrders, 4> const &orders, FlightBodies const &bodies) {
    FlightWorld world (aSeed);
    world.launch (0, rabbitAt (0., 0, fourFour, FlightTuning{}).at, 0., fourFour);
    run (world, orders, bodies, ticksIn (2.));
    return world.ship (0).p;
  };

  EXPECT_EQ (flownAlone (spared, target), flownAlone (pulled, {}));
  EXPECT_NE (flownAlone (pulled, target), flownAlone (pulled, {}));
  EXPECT_EQ (flownAlone (spared, both), flownAlone (pulled, other));
}
