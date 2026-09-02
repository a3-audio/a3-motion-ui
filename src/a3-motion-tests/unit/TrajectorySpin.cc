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

#include <a3-motion-engine/TrajectorySpin.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

using namespace a3;

namespace
{
constexpr float ticksPerBar = 128.f * 4.f; // ticksPerBeat x a four-beat bar
}

// Standing still is a value, not the absence of one: it is the middle of the
// control and the state a clip is in until somebody turns it.
TEST (TrajectorySpin, ZeroIsAStandstill)
{
  EXPECT_EQ (spinRevolutionsPerBar (0), 0.f);
  EXPECT_EQ (spinBarsPerRevolution (0), 0.f);
  EXPECT_EQ (advanceSpinPhase (0.25f, 0, ticksPerBar), 0.25f);
}

// The table the control steps through, written out. Eight steps from a
// revolution every thirty-two bars to four revolutions in one.
TEST (TrajectorySpin, EachStepHalvesTheBarsPerRevolution)
{
  float const expected[spinMaxStep]
      = { 32.f, 16.f, 8.f, 4.f, 2.f, 1.f, 0.5f, 0.25f };

  for (int step = 1; step <= spinMaxStep; ++step)
    {
      EXPECT_FLOAT_EQ (spinBarsPerRevolution (step),
                       expected[step - 1])
          << "step " << step;
      // The other direction is the same length, travelled the other way.
      EXPECT_FLOAT_EQ (spinBarsPerRevolution (-step), expected[step - 1]);
    }
}

TEST (TrajectorySpin, TheSignIsTheDirection)
{
  for (int step = 1; step <= spinMaxStep; ++step)
    {
      EXPECT_GT (spinRevolutionsPerBar (step), 0.f);
      EXPECT_FLOAT_EQ (spinRevolutionsPerBar (-step),
                       -spinRevolutionsPerBar (step));
    }
}

// A step past the end of the table is the end of the table. The control
// clamps, but the table is what anything else reads.
TEST (TrajectorySpin, StepsBeyondTheTableAreClamped)
{
  EXPECT_FLOAT_EQ (spinRevolutionsPerBar (spinMaxStep + 3),
                   spinRevolutionsPerBar (spinMaxStep));
  EXPECT_FLOAT_EQ (spinRevolutionsPerBar (-spinMaxStep - 3),
                   spinRevolutionsPerBar (-spinMaxStep));
}

// The point of tying this to the clock: a revolution is a whole number of
// bars, so it ends where it started, on a bar line.
TEST (TrajectorySpin, AWholeRevolutionTakesItsBarsExactly)
{
  for (int step = 1; step <= spinMaxStep; ++step)
    {
      auto const ticks = static_cast<int> (
          std::lround (spinBarsPerRevolution (step) * ticksPerBar));

      auto phase = 0.f;
      for (int tick = 0; tick < ticks; ++tick)
        phase = advanceSpinPhase (phase, step, ticksPerBar);

      // Back to the start, having gone all the way round once. Compared on
      // the circle: a hair short of a full turn is a hair from where it
      // started, and 0.9999 is not near 0 on a number line.
      auto const fromStart = juce::jmin (phase, 1.f - phase);
      EXPECT_NEAR (fromStart, 0.f, 1e-3f) << "step " << step;
    }
}

TEST (TrajectorySpin, ThePhaseStaysInOneRevolution)
{
  auto phase = 0.f;
  for (int tick = 0; tick < 10000; ++tick)
    {
      phase = advanceSpinPhase (phase, spinMaxStep, ticksPerBar);
      ASSERT_GE (phase, 0.f);
      ASSERT_LT (phase, 1.f);
    }

  // ... turning the other way too, where a naive fmod goes negative.
  phase = 0.f;
  for (int tick = 0; tick < 10000; ++tick)
    {
      phase = advanceSpinPhase (phase, -spinMaxStep, ticksPerBar);
      ASSERT_GE (phase, 0.f);
      ASSERT_LT (phase, 1.f);
    }
}

// The one thing the rotation must not touch. The 2D radius is the elevation,
// so a spin that changes it would walk the trajectory up and down the sphere
// instead of around it.
TEST (TrajectorySpin, SpinningKeepsEveryPointAtItsHeight)
{
  Pos const points[] = { Pos::fromCartesian (1.f, 0.f, 0.f),
                         Pos::fromCartesian (0.f, 0.6f, 0.f),
                         Pos::fromCartesian (-0.3f, 0.4f, 0.f),
                         Pos::fromCartesian (0.f, 0.f, 0.f) };

  for (auto const &p : points)
    {
      auto const before = std::hypot (p.x (), p.y ());
      for (float phase : { 0.f, 0.125f, 0.25f, 0.5f, 0.75f, 0.999f })
        {
          auto const spun = spinPosition (p, phase);
          EXPECT_NEAR (std::hypot (spun.x (), spun.y ()), before, 1e-5f);
        }
    }
}

// Pinned as what the eye sees, not as a sign convention, because the sign
// convention is the part that is easy to get backwards: cartesian2DHOA2JUCE
// puts an HOA position on screen at { -y, -x }, so (1, 0) is drawn at the top
// of the sphere and (0, -1) at its right. A quarter turn of a positive spin
// therefore has to take the first to the second — top to right, clockwise,
// which is the way the control was turned.
TEST (TrajectorySpin, AQuarterTurnRightGoesFromTheTopToTheRight)
{
  auto const spun
      = spinPosition (Pos::fromCartesian (1.f, 0.f, 0.f), 0.25f);

  EXPECT_NEAR (spun.x (), 0.f, 1e-5f);
  EXPECT_NEAR (spun.y (), -1.f, 1e-5f);

  // ... and turning the control the other way goes the other way.
  auto const other
      = spinPosition (Pos::fromCartesian (1.f, 0.f, 0.f), -0.25f);
  EXPECT_NEAR (other.y (), 1.f, 1e-5f);
}

TEST (TrajectorySpin, AFullRevolutionIsNoTurnAtAll)
{
  auto const p = Pos::fromCartesian (0.3f, -0.7f, 0.f);
  auto const spun = spinPosition (p, 1.f);

  EXPECT_NEAR (spun.x (), p.x (), 1e-5f);
  EXPECT_NEAR (spun.y (), p.y (), 1e-5f);
}


// What the spin is for, checked through the projection playback actually
// uses rather than only in the flat disc. Turning the recorded point moves
// the sound around the room at the height it was played in at — if this ever
// stops holding, a spinning clip walks up the sphere towards the pole.
TEST (TrajectorySpin, ThroughTheSphereItTurnsTheAzimuthAndNothingElse)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 0.5f;

  Pos const points[] = { Pos::fromCartesian (0.8f, 0.f, 0.f),
                         Pos::fromCartesian (0.f, 0.35f, 0.f),
                         Pos::fromCartesian (-0.5f, 0.5f, 0.f) };

  for (auto const &p : points)
    {
      auto const still = heightMap.mapTo3D (p, params);

      for (float phase : { 0.1f, 0.25f, 0.5f, 0.8f })
        {
          auto const spun
              = heightMap.mapTo3D (spinPosition (p, phase), params);

          EXPECT_NEAR (spun.elevation (), still.elevation (), 1e-4f)
              << "phase " << phase;
          EXPECT_NEAR (spun.distance (), still.distance (), 1e-4f)
              << "phase " << phase;

          // ... and the azimuth moved by exactly the phase, downwards: HOA
          // azimuth counts the way the maths does and the screen mirrors it,
          // so a clockwise turn on screen is a falling azimuth. Degrees, and
          // it wraps, so the two are compared on the circle.
          auto const turned = spun.azimuth () - still.azimuth ();
          auto const difference
              = std::fmod (std::abs (turned + phase * 360.f) + 360.f, 360.f);
          EXPECT_TRUE (difference < 0.05f
                       || std::abs (difference - 360.f) < 0.05f)
              << "phase " << phase << ", turned by " << turned;
        }
    }
}
