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

#include <a3-motion-engine/TrajectoryShape.hh>

#include <cmath>

using namespace a3;

namespace
{

// A trajectory drawn with a finger: 64 ticks around a circle, every step the
// same small size.
std::vector<Pos>
drawnCircle (int numTicks = 64)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < numTicks; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi
                     * static_cast<float> (i) / static_cast<float> (numTicks);
      ticks.push_back (Pos::fromCartesian (std::cos (a) * 0.6f,
                                           std::sin (a) * 0.6f, 0.5f));
    }
  return ticks;
}

// A trajectory tapped in: four positions, each held until the next tap. This
// is what a finished take looks like after the seams are closed — every tick
// is valid, so nothing about "invalid ticks" can tell it apart from a drawn
// one.
std::vector<Pos>
tappedCorners (int holdTicks = 16)
{
  std::vector<Pos> const corners
      = { Pos::fromCartesian (0.6f, 0.6f, 0.5f),
          Pos::fromCartesian (-0.6f, 0.6f, 0.5f),
          Pos::fromCartesian (-0.6f, -0.6f, 0.5f),
          Pos::fromCartesian (0.6f, -0.6f, 0.5f) };

  std::vector<Pos> ticks;
  for (auto const &corner : corners)
    for (int i = 0; i < holdTicks; ++i)
      ticks.push_back (corner);
  return ticks;
}

TEST (TrajectoryShape, ADrawnTrajectoryHasNoJumps)
{
  EXPECT_TRUE (trajectoryJumps (drawnCircle ()).empty ());
  EXPECT_FALSE (isTappedTrajectory (drawnCircle ()));
}

// The tick where a tap lands is a teleport, not a movement. Four taps in a
// ring means four of them — the last one being the wrap back to the first.
TEST (TrajectoryShape, EveryTapIsAJump)
{
  auto const jumps = trajectoryJumps (tappedCorners ());
  EXPECT_EQ (jumps.size (), 4u);
  EXPECT_TRUE (isTappedTrajectory (tappedCorners ()));
}

// The symptom: a finished tap take has no invalid ticks left, so the old test
// for "is this a jump pattern" said no and drew straight lines between the
// taps.
TEST (TrajectoryShape, AFinishedTapTakeHasNoInvalidTicksToGoOn)
{
  for (auto const &tick : tappedCorners ())
    ASSERT_TRUE (tick.isValid ());
}

// Four taps must read as four dots, not as sixty-four.
TEST (TrajectoryShape, EachHeldPositionBecomesOneDot)
{
  auto const held = trajectoryPlateaus (tappedCorners ());
  EXPECT_EQ (held.size (), 4u);
}

TEST (TrajectoryShape, ADrawnTrajectoryStaysOneSegment)
{
  auto const segments = trajectorySegments (drawnCircle ());
  ASSERT_EQ (segments.size (), 1u);
  EXPECT_EQ (segments[0].size (), 64u);
}

// A drawn trajectory with one teleport in the middle is two strokes, not one
// stroke with a chord across it.
TEST (TrajectoryShape, ADrawnTrajectoryIsCutAtItsJump)
{
  auto ticks = drawnCircle (32);
  for (int i = 16; i < 32; ++i)
    ticks[static_cast<size_t> (i)]
        += Pos::fromCartesian (0.9f, 0.9f, 0.f);

  auto const segments = trajectorySegments (ticks);
  EXPECT_EQ (segments.size (), 2u);
  EXPECT_FALSE (isTappedTrajectory (ticks))
      << "one jump in an otherwise drawn line does not make it a tap pattern";
}

// A gap in the data cuts it too — that behaviour was already there and has to
// survive.
TEST (TrajectoryShape, MissingTicksStillCutTheLine)
{
  auto ticks = drawnCircle (32);
  ticks[10] = Pos::invalid;
  ticks[11] = Pos::invalid;

  auto const segments = trajectorySegments (ticks);
  EXPECT_EQ (segments.size (), 2u);
}

// A drawn trajectory that pauses stands still most of the time too, but its
// standing still is spread over the whole line rather than gathered at a few
// taps. Drawing it as dots would throw away the line.
TEST (TrajectoryShape, ADrawnTrajectoryThatPausesIsStillDrawn)
{
  auto ticks = drawnCircle (32);
  auto const restingPlace = ticks.back ();
  for (int i = 0; i < 96; ++i)
    ticks.push_back (restingPlace);

  EXPECT_FALSE (isTappedTrajectory (ticks));
}

// One tap, held for the whole loop, is one dot.
TEST (TrajectoryShape, ASingleHeldPositionIsOneDot)
{
  std::vector<Pos> const ticks (
      64, Pos::fromCartesian (0.4f, 0.2f, 0.5f));

  EXPECT_TRUE (isTappedTrajectory (ticks));
  EXPECT_EQ (trajectoryPlateaus (ticks).size (), 1u);
}

TEST (TrajectoryShape, AnEmptyPatternHasNothingToShow)
{
  EXPECT_TRUE (trajectorySegments ({}).empty ());
  EXPECT_TRUE (trajectoryJumps ({}).empty ());
  EXPECT_TRUE (trajectoryPlateaus ({}).empty ());
  EXPECT_FALSE (isTappedTrajectory ({}));
}

}
