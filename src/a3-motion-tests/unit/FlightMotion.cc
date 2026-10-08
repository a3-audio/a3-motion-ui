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

#include <a3-motion-engine/TrajectorySpin.hh>
#include <a3-motion-engine/flight/FlightMotion.hh>

#include <algorithm>
#include <cmath>

using namespace a3;

// An action's Motion keys on a flying ship (FPV): rates in beats,
// each held under what a room can follow (one lap per two beats; height at
// most half as fast).

namespace
{
constexpr int fourFour = 4;
constexpr int threeFour = 3;

FlightMotion
spinning (int step)
{
  FlightMotion motion;
  motion.spin = step;
  return motion;
}

/** The fastest a sampled quantity changes per beat over `beats` beats. */
template <typename Sample>
double
fastestPerBeat (Sample sample, double beats)
{
  constexpr double step = 1e-3;
  auto fastest = 0.;
  for (auto t = 0.; t < beats; t += step)
    fastest = std::max (fastest, std::abs (sample (t + step) - sample (t)) / step);
  return fastest;
}
}

TEST (FlightMotion, NoMotionIsNoMotion)
{
  FlightMotion const none;
  FlightTuning const tuning;
  EXPECT_FALSE (none.any ());
  EXPECT_FALSE (none.movesTheFloor ());
  EXPECT_FLOAT_EQ (wantedLapsPerBeat (none, fourFour, tuning), baseLapsPerBeat (fourFour, tuning));
  EXPECT_FLOAT_EQ (swellScale (none, 3., fourFour, tuning), 1.f);
  EXPECT_FLOAT_EQ (swayDegrees (none, 3., fourFour, tuning), 0.f);
  auto const lean = flightLean (none, 3., fourFour, tuning);
  EXPECT_FLOAT_EQ (lean.tilt, 0.f);
  EXPECT_FLOAT_EQ (lean.roll, 0.f);
}

TEST (FlightMotion, TheBasePathIsALapInFourBars)
{
  EXPECT_FLOAT_EQ (baseLapsPerBeat (fourFour, FlightTuning{}), 1.f / 16.f);
}

TEST (FlightMotion, ASpinSetsTheLap)
{
  FlightTuning const tuning;
  EXPECT_FLOAT_EQ (std::abs (wantedLapsPerBeat (spinning (3), fourFour, tuning)), 1.f / 32.f)
      << "|3| is eight bars a lap";
  EXPECT_FLOAT_EQ (std::abs (wantedLapsPerBeat (spinning (6), fourFour, tuning)), 1.f / 4.f)
      << "|6| is a bar a lap";
  EXPECT_FLOAT_EQ (wantedLapsPerBeat (spinning (0), fourFour, tuning), 0.f)
      << "0 stands the orbit, as it stands a clip's spin";
}

// The sign is the clip's: spinPosition() is the one place that decides which
// way a positive spin looks, and a ship must turn the same way.
TEST (FlightMotion, APositiveSpinRunsTheWayAClipsSpinTurns)
{
  auto const clipTurn
      = spinPosition (Pos::fromCartesian (1.f, 0.f, 0.f), 0.01f).y ();
  auto const shipLaps = wantedLapsPerBeat (spinning (5), fourFour, FlightTuning{});
  EXPECT_EQ (std::signbit (shipLaps), std::signbit (clipTurn));
  EXPECT_NE (std::signbit (wantedLapsPerBeat (spinning (-5), fourFour, FlightTuning{})),
             std::signbit (shipLaps));
}

TEST (FlightMotion, ThePaceScalesTheLap)
{
  FlightTuning const tuning;
  FlightMotion slower;
  slower.speedLog2 = 1;
  FlightMotion faster;
  faster.speedLog2 = -1;
  auto const base = baseLapsPerBeat (fourFour, tuning);
  EXPECT_FLOAT_EQ (wantedLapsPerBeat (slower, fourFour, tuning), base / 2.f);
  EXPECT_FLOAT_EQ (wantedLapsPerBeat (faster, fourFour, tuning), base * 2.f);
}

// Past one lap per two beats a room stops hearing a path.
TEST (FlightMotion, TheLapNeverOutrunsTheEar)
{
  FlightTuning const tuning;
  for (auto const step : { 8, -8, 7, -7 })
    {
      auto motion = spinning (step);
      motion.speedLog2 = -7;
      EXPECT_LE (std::abs (wantedLapsPerBeat (motion, fourFour, tuning)) * 360.f,
                 tuning.angularCapDegreesPerBeat + 1e-3f)
          << step;
    }
}

TEST (FlightMotion, TheSwellBreathesOutAndBack)
{
  FlightTuning const tuning;
  FlightMotion out;
  out.swell = 5; // two bars a breath
  auto const longAxis = tuning.orbitRadius * (1.f + tuning.orbitEccentricity);
  EXPECT_FLOAT_EQ (swellScale (out, 0., fourFour, tuning), 1.f);
  EXPECT_NEAR (swellScale (out, 4., fourFour, tuning), tuning.swellOuterReach / longAxis, 1e-4);
  EXPECT_NEAR (swellScale (out, 8., fourFour, tuning), 1.f, 1e-4);

  FlightMotion in;
  in.swell = -5;
  EXPECT_NEAR (swellScale (in, 4., fourFour, tuning), tuning.swellInnerScale, 1e-4);
}

TEST (FlightMotion, APositiveSwayGoesDownFirst)
{
  FlightTuning const tuning;
  FlightMotion sway;
  sway.sway = 5;
  EXPECT_NEAR (swayDegrees (sway, 4., fourFour, tuning), -tuning.swayTravelDegrees, 1e-3);
  sway.sway = -5;
  EXPECT_NEAR (swayDegrees (sway, 4., fourFour, tuning), tuning.swayTravelDegrees, 1e-3);
}

// Height moves at most half as fast as a turn.
TEST (FlightMotion, TheSwayStaysUnderTheVerticalCap)
{
  FlightTuning const tuning;
  FlightMotion fastest;
  fastest.sway = 8;
  auto const rate = fastestPerBeat (
      [&] (double t) { return static_cast<double> (swayDegrees (fastest, t, fourFour, tuning)); },
      2.);
  EXPECT_LE (rate, tuning.verticalCapDegreesPerBeat * 1.01);
}

TEST (FlightMotion, AStandingLeanIsTheTilt)
{
  FlightMotion leant;
  leant.tilt = 1.f;
  leant.roll = -0.5f;
  auto const lean = flightLean (leant, 7., fourFour, FlightTuning{});
  EXPECT_FLOAT_EQ (lean.tilt, 1.f);
  EXPECT_FLOAT_EQ (lean.roll, -0.5f);
}

TEST (FlightMotion, ALeanTurnsRoundNoFasterThanTheCap)
{
  FlightTuning const tuning;
  FlightMotion tumbling;
  tumbling.tiltSweep = 8;
  auto const quarterTurnsPerBeat = fastestPerBeat (
      [&] (double t) {
        // Unwrapped against the start, so the ring's seam is not a jump.
        return static_cast<double> (flightLean (tumbling, t, fourFour, tuning).tilt);
      },
      0.4);
  EXPECT_LE (quarterTurnsPerBeat * 90., tuning.angularCapDegreesPerBeat * 1.01);
}

// The caps are beats, not bars.
TEST (FlightMotion, TheCapsHoldInThreeFour)
{
  FlightTuning const tuning;
  EXPECT_LE (std::abs (wantedLapsPerBeat (spinning (8), threeFour, tuning)) * 360.f,
             tuning.angularCapDegreesPerBeat + 1e-3f);
  EXPECT_FLOAT_EQ (cycleBeats (8, threeFour, 360.f / tuning.angularCapDegreesPerBeat), 2.f)
      << "a quarter bar of 3/4 is under the two beats a turn takes";
  EXPECT_FLOAT_EQ (cycleBeats (5, threeFour, 0.f), 6.f);
}

TEST (FlightMotion, OnlySpinPaceAndSwellMoveTheFloor)
{
  FlightMotion heardOnly;
  heardOnly.sway = 3;
  heardOnly.tilt = 1.f;
  heardOnly.rollSweep = 4;
  EXPECT_TRUE (heardOnly.any ());
  EXPECT_FALSE (heardOnly.movesTheFloor ());
  heardOnly.swell = 2;
  EXPECT_TRUE (heardOnly.movesTheFloor ());
}
