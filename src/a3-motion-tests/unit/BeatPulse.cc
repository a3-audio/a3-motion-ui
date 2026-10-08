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

#include <a3-motion-engine/flight/BeatPulse.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

using namespace a3;

namespace
{

constexpr int fourFour = 4;
constexpr int ticksPerBeat = TempoClock::getTicksPerBeat ();

/** The pulse before the gate: gravity breathes on every beat. */
FlightTuning
breathingPulse ()
{
  FlightTuning tuning;
  tuning.gravityOnlyOnTheOne = false;
  return tuning;
}

float
pulseAt (int beat, int tick, FlightTuning const &tuning = breathingPulse ())
{
  return gravityPulse (Measure (0, beat, tick), fourFour, tuning);
}

float
gatedAt (int beat, int tick)
{
  return gravityPulse (Measure (0, beat, tick), fourFour, FlightTuning{});
}

}

// The MJ lab's gate (2026-10-08, playbook rule 17): gravity pulls only during
// beat 1 of each bar, so every bend lands on the one. It is the default.
TEST (BeatPulse, GravityPullsOnlyDuringTheOne)
{
  for (auto tick : { 0, ticksPerBeat / 2, ticksPerBeat - 1 })
    {
      EXPECT_FLOAT_EQ (gatedAt (0, tick), 1.f) << "the one, tick " << tick;
      for (auto beat = 1; beat < fourFour; ++beat)
        EXPECT_FLOAT_EQ (gatedAt (beat, tick), 0.f)
            << "beat " << beat + 1 << ", tick " << tick;
    }
}

TEST (BeatPulse, TheGateFollowsTheBar)
{
  // The second bar's one pulls again; a three-beat bar gates its own one.
  EXPECT_FLOAT_EQ (gravityPulse (Measure (1, 0, 10), fourFour, FlightTuning{}), 1.f);
  EXPECT_FLOAT_EQ (gravityPulse (Measure (0, 3, 0), 3, FlightTuning{}), 1.f);
  EXPECT_FLOAT_EQ (gravityPulse (Measure (0, 2, 0), 3, FlightTuning{}), 0.f);
}

TEST (BeatPulse, TheGateIsTheDefault)
{
  EXPECT_TRUE (FlightTuning{}.gravityOnlyOnTheOne);
}

TEST (BeatPulse, TheOnsetPullsHarderThanMidBeat)
{
  EXPECT_GT (pulseAt (1, 0), pulseAt (1, ticksPerBeat / 2));
}

TEST (BeatPulse, TheOneOfTheBarPullsHardest)
{
  EXPECT_GT (pulseAt (0, 0), pulseAt (2, 0));
}

TEST (BeatPulse, ItRelaxesToOneBeforeTheNextBeat)
{
  EXPECT_NEAR (pulseAt (0, ticksPerBeat - 1), 1.f, 1e-3f);
  EXPECT_NEAR (pulseAt (3, ticksPerBeat - 1), 1.f, 1e-3f);
}

TEST (BeatPulse, DepthZeroIsConstant)
{
  auto flat = breathingPulse ();
  flat.pulseDepth = 0.f;
  flat.pulseDownbeatDepth = 0.f;
  for (auto beat = 0; beat < fourFour; ++beat)
    for (auto tick = 0; tick < ticksPerBeat; ++tick)
      EXPECT_FLOAT_EQ (pulseAt (beat, tick, flat), 1.f)
          << "beat " << beat << " tick " << tick;
}

TEST (BeatPulse, NeverBelowOne)
{
  for (auto beat = 0; beat < fourFour; ++beat)
    for (auto tick = 0; tick < ticksPerBeat; ++tick)
      EXPECT_GE (pulseAt (beat, tick), 1.f)
          << "beat " << beat << " tick " << tick;
}

TEST (BeatPulse, ANegativeTickStaysInRange)
{
  // A measure just before a beat (a tempo nudge, an EXT clock correcting
  // backwards) must not pull harder than the onset does.
  auto const t = breathingPulse ();
  for (auto beat = 0; beat < fourFour; ++beat)
    for (auto tick : { -1, -64, -127, -200 })
      {
        auto const pulse = pulseAt (beat, tick, t);
        EXPECT_GE (pulse, 1.f) << "beat " << beat << " tick " << tick;
        EXPECT_LE (pulse, 1.f + t.pulseDownbeatDepth)
            << "beat " << beat << " tick " << tick;
      }
}

// What the floor draws: under the gate the discs swell on the one and rest at
// their size on every other beat, never smaller (a pull of 0 is not a shrink).
TEST (BeatPulse, TheDiscsSwellOnTheOneAndRestOtherwise)
{
  FlightTuning const gate;
  auto const drawnAt = [&gate] (int beat, int tick) {
    return drawnPulse (Measure (0, beat, tick), fourFour, gate);
  };
  EXPECT_GT (drawnAt (0, 0), drawnAt (0, ticksPerBeat / 2));
  EXPECT_GT (drawnAt (0, ticksPerBeat / 2), 1.f);
  for (auto beat = 1; beat < fourFour; ++beat)
    for (auto tick : { 0, ticksPerBeat / 2 })
      EXPECT_FLOAT_EQ (drawnAt (beat, tick), 1.f) << "beat " << beat + 1;
}

TEST (BeatPulse, WithoutTheGateTheDiscsBreatheWithThePull)
{
  auto const t = breathingPulse ();
  for (auto beat = 0; beat < fourFour; ++beat)
    EXPECT_FLOAT_EQ (drawnPulse (Measure (0, beat, 5), fourFour, t),
                     pulseAt (beat, 5, t));
}
