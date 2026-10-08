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
#include <a3-motion-engine/flight/BeatPulse.hh>
#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/ShipDynamics.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <optional>
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

/** Ship 0 for `bars`, chasing its rabbit with the bodies' gravity, from the
 *  rabbit's own place and pace. One sample a tick. The gravity's pulse is the
 *  one the engine plays (the gate: pulling only on the one), unless a fixed
 *  `pulse` is given. */
std::vector<Sample>
fly (float bars, FlightBodies const &bodies, std::optional<float> pulse = {})
{
  FlightTuning const tuning;
  auto const dt = tickBeats ();
  auto const start = rabbitAt (0., 0, fourFour, tuning);
  ShipState ship{ start.at, start.velocity };
  std::vector<Sample> path;
  for (size_t i = 0; i < ticksIn (beatsIn (bars)); ++i)
    {
      auto const now = static_cast<double> (i) * dt;
      auto const pulling
          = pulse ? *pulse
                  : gravityPulse (Measure (0, 0, static_cast<int> (i)), fourFour,
                                  tuning);
      ShipForces const forces{ rabbitAt (now, 0, fourFour, tuning),
                               gravityAt (ship.p, bodies, pulling, tuning)
                                   + deadZonePush (ship.p, bodies, tuning),
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

// ---- What a guest hears ----
//
// The room's own bars: a ship must not park on the rim, and must keep out of
// a dead zone. The planets' bends are pinned further below, as quiet ones.

namespace
{

// A ship held in one place for longer than a bar is heard as parked, not
// flying (the lab's failure case: ships that park overhead).
constexpr double parkedBeats = fourFour;

/** Eight places round the path, a quarter bar after each other bar: where
 *  the rabbit is at these beats. The first lies a beat and a half after the
 *  start, so the ship is in full flight when it arrives. */
std::vector<double>
eightPlacesRoundThePath ()
{
  std::vector<double> beats;
  for (auto i = 0; i < 8; ++i)
    beats.push_back (1.5 + 2. * i);
  return beats;
}

/** The rabbit's place at `beats`, moved `outwards` along the radius
 *  (negative: inwards). */
Vec2
besideThePath (double beats, float outwards)
{
  auto const onThePath = rabbitOf (beats);
  return onThePath
         + onThePath * (outwards / onThePath.getDistanceFromOrigin ());
}

float
widestApart (std::vector<Sample> const &a, std::vector<Sample> const &b)
{
  auto widest = 0.f;
  for (size_t i = 0; i < std::min (a.size (), b.size ()); ++i)
    widest = std::max (widest, a[i].ship.p.getDistanceFrom (b[i].ship.p));
  return widest;
}

/** How many laps round the room's centre the ship turns over `path`. */
float
lapsRoundTheRoom (std::vector<Sample> const &path)
{
  auto turned = 0.f;
  for (size_t i = 1; i < path.size (); ++i)
    {
      auto const a = path[i - 1].ship.p;
      auto const b = path[i].ship.p;
      turned += std::remainder (std::atan2 (b.y, b.x) - std::atan2 (a.y, a.x),
                                twoPi);
    }
  return std::abs (turned) / twoPi;
}

/** The longest the ship stays on the rim without a break, in beats. */
double
longestOnTheRim (std::vector<Sample> const &path)
{
  constexpr float onTheRim = 0.98f;
  size_t longest = 0, run = 0;
  for (auto const &sample : path)
    {
      run = sample.ship.p.getDistanceFromOrigin () >= onTheRim ? run + 1 : 0;
      longest = std::max (longest, run);
    }
  return static_cast<double> (longest) * tickBeats ();
}

}

// ---- Quiet planets (the maintainer's decision, 2026-10-08) ----
//
// The planets say *where* the sound goes; the breath carries the motion. So
// they are light and pull only on the one, and their bends are small: a crowd
// beside the path moves the ship 0.054-0.167 floor units, a hotspot
// 0.085-0.210, under the 0.30 the ear needs to hear a bend on its own
// (measured 2026-10-08). These tests pin that they still act, where and when
// they should; they do not ask them to be heard.

// Half the smallest crowd bend measured (0.054): a change that halves the
// planets' pull fails, the quiet design passes.
constexpr float measurableBend = 0.03f;

TEST (FlightGravity, ACrowdOrAHotspotBesideThePathStillMovesTheShip)
{
  FlightTuning const t;
  auto const empty = fly (8.f, {});
  for (auto const mass : { t.crowdMass, t.hotspotMass })
    for (auto const beats : eightPlacesRoundThePath ())
      {
        FlightBody const body{ besideThePath (beats, -0.1f), mass };
        EXPECT_GE (widestApart (fly (8.f, bodiesOf ({ body })), empty), measurableBend)
            << "mass " << mass << " where the rabbit is at beat " << beats;
      }
}

// Towards it, not away: the ship comes closer to a crowd than the empty
// room's flight does, at most places round the path (7 of 8 measured; near
// one place the steering's catch-up outweighs the light pull).
TEST (FlightGravity, APlanetDrawsTheShipTowardsIt)
{
  FlightTuning const t;
  constexpr int mostOfEight = 6;
  auto const empty = fly (8.f, {});
  for (auto const mass : { t.groupMass, t.crowdMass, t.hotspotMass })
    {
      auto closer = 0;
      for (auto const beats : eightPlacesRoundThePath ())
        {
          auto const at = besideThePath (beats, -0.1f);
          if (closestApproach (fly (8.f, bodiesOf ({ { at, mass } })), at)
              < closestApproach (empty, at))
            ++closer;
        }
      EXPECT_GE (closer, mostOfEight) << "mass " << mass;
    }
}

// The bends land on the one: the flight with a crowd leaves the empty room's
// flight first during a beat 1, and over beats 2-4 nothing pulls.
TEST (FlightGravity, APlanetActsOnlyOnTheOne)
{
  FlightTuning const t;
  auto const empty = fly (4.f, {});
  for (auto const beats : eightPlacesRoundThePath ())
    {
      auto const path
          = fly (4.f, bodiesOf ({ { besideThePath (beats, -0.1f), t.crowdMass } }));
      size_t i = 0;
      while (i < path.size () && path[i].ship.p == empty[i].ship.p)
        ++i;
      ASSERT_LT (i, path.size ()) << "the crowd never moved the ship";
      auto const beatInBar
          = static_cast<int> (std::floor (path[i].beats - tickBeats ())) % fourFour;
      EXPECT_EQ (beatInBar, 0) << "first moved at beat " << path[i].beats;
    }
}

TEST (FlightGravity, NothingPullsOnBeatsTwoToFour)
{
  FlightTuning const t;
  auto const bodies = bodiesOf ({ { { 0.3f, 0.3f }, t.hotspotMass },
                                  { { -0.4f, 0.1f }, t.crowdMass } });
  auto const ticksPerBeat = TempoClock::getTicksPerBeat ();
  for (auto beat = 0; beat < fourFour; ++beat)
    for (auto tick : { 0, ticksPerBeat / 2, ticksPerBeat - 1 })
      {
        auto const pulse = gravityPulse (Measure (0, beat, tick), fourFour, t);
        auto const pull = gravityAt ({ 0.6f, 0.2f }, bodies, pulse, t);
        if (beat == 0)
          EXPECT_GT (pull.getDistanceFromOrigin (), 0.f) << "the one, tick " << tick;
        else
          EXPECT_EQ (pull, (Vec2{ 0.f, 0.f })) << "beat " << beat + 1 << ", tick " << tick;
      }
}

// Even a light group makes lap three differ from lap two: measured 0.044 with
// a G, where the empty room repeats itself to 0.0000. Half of it is the bar.
TEST (FlightGravity, TwoLapsAreNotTheSameWithAGroupThere)
{
  auto const empty = lapTwoAgainstLapThree (fly (16.f, {}));
  auto const withAGroup = lapTwoAgainstLapThree (
      fly (16.f, bodiesOf ({ { { 0.3f, 0.3f }, FlightTuning{}.groupMass } })));
  EXPECT_LT (empty, 0.001f) << "the empty room repeats itself, as it should";
  EXPECT_GT (withAGroup, 0.02f);
}

TEST (FlightGravity, AHotspotOutsideTheRoomNeverPinsTheShipToTheRim)
{
  auto const path
      = fly (8.f, bodiesOf ({ { { -0.8f, -0.8f }, FlightTuning{}.hotspotMass } }));
  EXPECT_LE (longestOnTheRim (path), parkedBeats);
  // eight bars are two laps of the path (and a quarter of its precession)
  EXPECT_GE (lapsRoundTheRoom (path), 1.75f);
}


// "Not here" means the sound is not heard from there: the ship keeps its
// distance even where the rabbit runs straight through the zone. Most of the
// clearance, not all of it: at full speed a ship leans into the wall a little.
TEST (FlightGravity, ADeadZoneOnThePathKeepsTheShipOut)
{
  FlightTuning const tuning;
  constexpr float mostOfTheClearance = 0.6f;
  for (auto const beats : eightPlacesRoundThePath ())
    {
      auto const deadZone = rabbitOf (beats);
      auto const path
          = fly (8.f, bodiesOf ({ { deadZone, tuning.deadZoneMass } }));
      EXPECT_GE (closestApproach (path, deadZone),
                 mostOfTheClearance * tuning.deadZoneClearance)
          << "placed where the rabbit is at beat " << beats;
    }
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
