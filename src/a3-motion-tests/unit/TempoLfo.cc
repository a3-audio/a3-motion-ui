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

#include <cmath>

#include <JuceHeader.h>

#include <a3-motion-engine/TempoLfo.hh>

using namespace a3;

namespace
{
constexpr float ticksPerBar = 128.f * 4.f; // ticksPerBeat x a four-beat bar
}


// Standing still is a value, not the absence of one: it is the middle of the
// control and the state a clip is in until somebody turns it.
TEST (TempoLfo, ZeroIsAStandstill)
{
  EXPECT_EQ (lfoCyclesPerBar (0), 0.f);
  EXPECT_EQ (lfoBarsPerCycle (0), 0.f);
  EXPECT_EQ (advanceLfoPhase (0.25f, 0, ticksPerBar), 0.25f);
}

// The table the control steps through, written out. Eight steps from a
// cycle every thirty-two bars to four cycles in one.
TEST (TempoLfo, EachStepHalvesTheBarsPerCycle)
{
  float const expected[lfoMaxStep]
      = { 32.f, 16.f, 8.f, 4.f, 2.f, 1.f, 0.5f, 0.25f };

  for (int step = 1; step <= lfoMaxStep; ++step)
    {
      EXPECT_FLOAT_EQ (lfoBarsPerCycle (step),
                       expected[step - 1])
          << "step " << step;
      // The other direction is the same length, travelled the other way.
      EXPECT_FLOAT_EQ (lfoBarsPerCycle (-step), expected[step - 1]);
    }
}

TEST (TempoLfo, TheSignIsTheDirection)
{
  for (int step = 1; step <= lfoMaxStep; ++step)
    {
      EXPECT_GT (lfoCyclesPerBar (step), 0.f);
      EXPECT_FLOAT_EQ (lfoCyclesPerBar (-step),
                       -lfoCyclesPerBar (step));
    }
}

// A step past the end of the table is the end of the table. The control
// clamps, but the table is what anything else reads.
TEST (TempoLfo, StepsBeyondTheTableAreClamped)
{
  EXPECT_FLOAT_EQ (lfoCyclesPerBar (lfoMaxStep + 3),
                   lfoCyclesPerBar (lfoMaxStep));
  EXPECT_FLOAT_EQ (lfoCyclesPerBar (-lfoMaxStep - 3),
                   lfoCyclesPerBar (-lfoMaxStep));
}

// The point of tying this to the clock: a cycle is a whole number of
// bars, so it ends where it started, on a bar line.
TEST (TempoLfo, AWholeCycleTakesItsBarsExactly)
{
  for (int step = 1; step <= lfoMaxStep; ++step)
    {
      auto const ticks = static_cast<int> (
          std::lround (lfoBarsPerCycle (step) * ticksPerBar));

      auto phase = 0.f;
      for (int tick = 0; tick < ticks; ++tick)
        phase = advanceLfoPhase (phase, step, ticksPerBar);

      // Back to the start, having gone all the way round once. Compared on
      // the circle: a hair short of a full turn is a hair from where it
      // started, and 0.9999 is not near 0 on a number line.
      auto const fromStart = juce::jmin (phase, 1.f - phase);
      EXPECT_NEAR (fromStart, 0.f, 1e-3f) << "step " << step;
    }
}

TEST (TempoLfo, ThePhaseStaysInOneCycle)
{
  auto phase = 0.f;
  for (int tick = 0; tick < 10000; ++tick)
    {
      phase = advanceLfoPhase (phase, lfoMaxStep, ticksPerBar);
      ASSERT_GE (phase, 0.f);
      ASSERT_LT (phase, 1.f);
    }

  // ... turning the other way too, where a naive fmod goes negative.
  phase = 0.f;
  for (int tick = 0; tick < 10000; ++tick)
    {
      phase = advanceLfoPhase (phase, -lfoMaxStep, ticksPerBar);
      ASSERT_GE (phase, 0.f);
      ASSERT_LT (phase, 1.f);
    }
}

// The travel curve. Out and back within one cycle, and — the part that
// matters to the eye — flat at both ends, so the turn does not read as the
// movement being yanked round.
TEST (TempoLfo, TheTravelGoesOutAndComesBack)
{
  EXPECT_NEAR (lfoTravel (0.f), 0.f, 1e-6f);
  EXPECT_NEAR (lfoTravel (0.5f), 1.f, 1e-6f);
  EXPECT_NEAR (lfoTravel (1.f), 0.f, 1e-6f);

  // Symmetric about the turn.
  for (float d : { 0.05f, 0.1f, 0.25f })
    EXPECT_NEAR (lfoTravel (0.5f - d), lfoTravel (0.5f + d), 1e-6f);

  // Nothing outside the range it is supposed to travel.
  for (int i = 0; i <= 100; ++i)
    {
      auto const v = lfoTravel (static_cast<float> (i) / 100.f);
      ASSERT_GE (v, 0.f);
      ASSERT_LE (v, 1.f);
    }

  // Flat at the ends: the first hundredth of the cycle moves a fraction of
  // what the hundredth around the middle does.
  auto const atTheEnd = lfoTravel (0.01f) - lfoTravel (0.f);
  auto const inTheMiddle = lfoTravel (0.26f) - lfoTravel (0.25f);
  EXPECT_LT (atTheEnd, inTheMiddle * 0.1f);
}

// A standstill leaves the value alone. Not "leaves it near where it was" —
// exactly alone, at every phase, so switching the sweep off cannot leave the
// parameter parked somewhere the knob does not say.
TEST (TempoLfo, AStandstillDoesNotTouchTheValue)
{
  for (float from : { 0.f, 0.25f, 0.5f, 1.f })
    for (int i = 0; i <= 20; ++i)
      EXPECT_EQ (lfoSweep (from, 0, static_cast<float> (i) / 20.f), from);
}

// Where it starts and where it goes: out of the value that was set, all the
// way to the end the sign points at, and back.
TEST (TempoLfo, TheSweepLeavesTheSetValueAndReturnsToIt)
{
  for (float from : { 0.f, 0.2f, 0.5f, 0.8f, 1.f })
    {
      EXPECT_NEAR (lfoSweep (from, 3, 0.f), from, 1e-6f);
      EXPECT_NEAR (lfoSweep (from, 3, 1.f), from, 1e-6f);
      EXPECT_NEAR (lfoSweep (from, -3, 0.f), from, 1e-6f);
      EXPECT_NEAR (lfoSweep (from, -3, 1.f), from, 1e-6f);

      // Halfway is the far end, whichever way the sign points.
      EXPECT_NEAR (lfoSweep (from, 3, 0.5f), 1.f, 1e-6f);
      EXPECT_NEAR (lfoSweep (from, -3, 0.5f), 0.f, 1e-6f);
    }
}

// The reason this shape was chosen over one that swings evenly either side of
// the set value: that one shrinks to nothing as the value nears a limit, so
// turning one control up quietly switches the other one off. This one reaches
// its end from anywhere.
TEST (TempoLfo, NoSettingLeavesTheSweepWithNowhereToGo)
{
  for (int i = 0; i <= 20; ++i)
    {
      auto const from = static_cast<float> (i) / 20.f;

      auto const outward = lfoSweep (from, 4, 0.5f);
      auto const inward = lfoSweep (from, -4, 0.5f);

      // One of the two always has the whole way to travel.
      EXPECT_NEAR (juce::jmax (std::abs (outward - from),
                               std::abs (inward - from)),
                   juce::jmax (from, 1.f - from), 1e-6f);
    }
}

// It is a 0..1 parameter at both ends of the journey and everywhere between.
TEST (TempoLfo, TheSweepStaysInRange)
{
  for (float from : { 0.f, 0.3f, 0.7f, 1.f })
    for (int step : { 1, 5, -1, -5 })
      for (int i = 0; i <= 100; ++i)
        {
          auto const v
              = lfoSweep (from, step, static_cast<float> (i) / 100.f);
          ASSERT_GE (v, 0.f);
          ASSERT_LE (v, 1.f);
        }
}
