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

#include <a3-motion-engine/util/Slew.hh>

#include <cmath>

using namespace a3;

TEST (Slew, AFullSweepTakesTheRampTime)
{
  // Ten milliseconds of a thirty-millisecond ramp is a third of the range.
  EXPECT_FLOAT_EQ (slewTowards (0.f, 1.f, 10., 30.), 1.f / 3.f);
  EXPECT_FLOAT_EQ (slewTowards (0.f, 1.f, 15., 30.), 0.5f);
}

TEST (Slew, ItLandsOnTheTargetExactlyRatherThanApproachingIt)
{
  // The contract of a channel value at rest: what the hand set, to the bit.
  // A one-pole filter would be the usual smoothing here and would never
  // arrive, leaving every knob a little off wherever it stopped.
  EXPECT_FLOAT_EQ (slewTowards (0.4f, 0.5f, 30., 30.), 0.5f);
  EXPECT_FLOAT_EQ (slewTowards (0.4f, 0.5f, 1000., 30.), 0.5f);

  // And it is *reached*, not merely approached: a full sweep in exact steps.
  auto value = 0.f;
  for (int step = 0; step < 3; ++step)
    value = slewTowards (value, 1.f, 10., 30.);
  EXPECT_FLOAT_EQ (value, 1.f);
}

TEST (Slew, ItNeverOvershoots)
{
  EXPECT_FLOAT_EQ (slewTowards (0.9f, 1.f, 100., 30.), 1.f);
  EXPECT_FLOAT_EQ (slewTowards (0.1f, 0.f, 100., 30.), 0.f);
}

TEST (Slew, ItRunsBothWays)
{
  EXPECT_FLOAT_EQ (slewTowards (1.f, 0.f, 10., 30.), 1.f - 1.f / 3.f);
  EXPECT_LT (slewTowards (0.8f, 0.2f, 5., 30.), 0.8f);
  EXPECT_GT (slewTowards (0.2f, 0.8f, 5., 30.), 0.2f);
}

TEST (Slew, NoRampMeansNoSmoothing)
{
  // Not an error: it is the switch that turns this off, and a caller should
  // not have to special-case it.
  EXPECT_FLOAT_EQ (slewTowards (0.f, 1.f, 10., 0.), 1.f);
  EXPECT_FLOAT_EQ (slewTowards (0.f, 1.f, 10., -5.), 1.f);
}

TEST (Slew, NoTimeMeansNothingMoves)
{
  // Two calls inside one clock resolution, or a clock that went backwards.
  EXPECT_FLOAT_EQ (slewTowards (0.25f, 1.f, 0., 30.), 0.25f);
  EXPECT_FLOAT_EQ (slewTowards (0.25f, 1.f, -10., 30.), 0.25f);
}

/** What the ramp is for: at the tick rates this actually runs at, a jump is
 *  broken into enough messages to stop being a click. A3 Core passes each one
 *  straight to a REAPER parameter with no interpolation, so the number of
 *  steps is the whole of what this side can do about it. */
TEST (Slew, AJumpBecomesEnoughStepsToStopBeingAClick)
{
  // 128 ticks a beat: about 7.8 ms a tick at 60 BPM, 2.6 at 180.
  for (double tickMillis : { 2.6, 3.9, 7.8 })
    {
      auto value = 0.f;
      auto steps = 0;
      while (value < 1.f)
        {
          value = slewTowards (value, 1.f, tickMillis, potSlewMillis);
          ++steps;
          ASSERT_LT (steps, 100) << "tick " << tickMillis;
        }

      EXPECT_GE (steps, 4) << "tick " << tickMillis;
    }
}
