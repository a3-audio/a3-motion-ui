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
                               gravityAt (ship.p, bodies, pulse, tuning)
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

// ---- What a guest hears ----
//
// The bars below are perceptual, not tuning: a ship is only "not boring" if
// the ear can tell. They come from auditory-motion-research.md part B and are
// met with room to spare, so the rig can retune FlightTuning without
// touching them.

namespace
{

// A bend is heard once the source sits >= ~25 deg away from where the bare
// path would have put it: B2 asks >= 20 deg in front and >= 30 deg to the
// sides. As a chord at the path's radius 0.7: 2 * 0.7 * sin (12.5 deg) = 0.30.
constexpr float audibleBend = 0.3f;

// A change of speed needs time to be heard: about 250 ms, half a beat at
// 120 BPM, is what the speed is averaged over (B5: integration ~336 ms).
constexpr double heardSpeedBeats = 0.5;

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

float
heardSpeed (std::vector<Sample> const &path, size_t at)
{
  auto const half = ticksIn (heardSpeedBeats / 2.);
  auto const from = at > half ? at - half : 0;
  auto const to = std::min (path.size (), at + half);
  auto sum = 0.f;
  for (auto i = from; i < to; ++i)
    sum += path[i].ship.v.getDistanceFromOrigin ();
  return sum / static_cast<float> (to - from);
}

/** How much faster than in the empty room the ship is heard while it passes
 *  `body` and is flung out: the largest ratio from a beat before the closest
 *  approach to two beats after it has left the body's neighbourhood. */
float
slingOf (FlightBody const &body)
{
  constexpr float neighbourhood = 0.15f;
  auto const bars = 6.f;
  auto const path = fly (bars, bodiesOf ({ body }));
  auto const empty = fly (bars, {});

  auto const closest = closestIndex (path, body.at);
  auto left = closest;
  while (left + 1 < path.size ()
         && path[left + 1].ship.p.getDistanceFrom (body.at) < neighbourhood)
    ++left;

  auto const from = closest > ticksIn (1.) ? closest - ticksIn (1.) : 0;
  auto const to = std::min (path.size (), left + ticksIn (2.));
  auto fastest = 0.f;
  for (auto i = from; i < to; ++i)
    fastest = std::max (fastest, heardSpeed (path, i) / heardSpeed (empty, i));
  return fastest;
}

/** Of sixteen passes (eight places, on the path and just outside it), how
 *  many fling the ship out audibly faster. */
int
audibleSlingshots (float mass)
{
  auto count = 0;
  for (auto const beats : eightPlacesRoundThePath ())
    for (auto const outwards : { 0.f, 0.08f })
      if (slingOf ({ besideThePath (beats, outwards), mass }) >= audibleSpeedUp)
        ++count;
  return count;
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

TEST (FlightGravity, ACrowdNearThePathBendsItAudibly)
{
  auto const empty = fly (8.f, {});
  for (auto const beats : eightPlacesRoundThePath ())
    {
      FlightBody const crowd{ besideThePath (beats, -0.1f), 2.f };
      EXPECT_GE (widestApart (fly (8.f, bodiesOf ({ crowd })), empty), audibleBend)
          << "a crowd just inside the path where the rabbit is at beat " << beats;
    }
}

// A slingshot has to speed the ship up by 1.5x to be heard as faster at all.
// Near a heavy group the flight is chaotic: some passes are held for a moment
// instead (that is the capture). So the bar is most passes, three in four.
TEST (FlightGravity, ACrowdOrAHotspotOnThePathSlingsTheShipOnMostPasses)
{
  constexpr int mostOfSixteen = 12;
  EXPECT_GE (audibleSlingshots (2.f), mostOfSixteen) << "a crowd";
  EXPECT_GE (audibleSlingshots (3.f), mostOfSixteen) << "a hotspot";
}

TEST (FlightGravity, AHotspotOutsideTheRoomNeverPinsTheShipToTheRim)
{
  auto const path = fly (8.f, bodiesOf ({ { { -0.8f, -0.8f }, 3.f } }));
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
      auto const path = fly (8.f, bodiesOf ({ { deadZone, -2.f } }));
      EXPECT_GE (closestApproach (path, deadZone),
                 mostOfTheClearance * tuning.deadZoneClearance)
          << "placed where the rabbit is at beat " << beats;
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
