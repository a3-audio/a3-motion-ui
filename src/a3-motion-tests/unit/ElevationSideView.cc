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

#include <JuceHeader.h>

#include <a3-motion-ui/components/ElevationSideView.hh>

using namespace a3;

namespace
{
constexpr float epsilon = 1e-4f;
}

// Height is the whole reason this picture exists, and it is measured the same
// way the base line and the clip cuts are: 0 at the ceiling, 1 at the floor.
// A point drawn at any other scale would not sit on the line it belongs to.
TEST (ElevationSideView, HeightIsTheSameFractionTheLinesAreDrawnAt)
{
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, 0.f, 1.f)).frac, 0.f,
               epsilon);
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (1.f, 0.f, 0.f)).frac,
               0.5f, epsilon);
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, 0.f, -1.f)).frac,
               1.f, epsilon);
}

// And left is left. The sphere above puts the room's +y to the left of the
// screen; turning your head down to this circle must not turn the room with
// it, or a sound heard on the left would be drawn on the right.
TEST (ElevationSideView, LeftAndRightAgreeWithTheSphereAbove)
{
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, 1.f, 0.f)).across,
               -1.f, epsilon);
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, -1.f, 0.f)).across,
               1.f, epsilon);
}

// The viewer stands at the near edge of the overhead picture and looks into
// it, so the room's +x -- the top of that picture -- is the far side.
TEST (ElevationSideView, TheFarSideOfTheRoomIsBehind)
{
  EXPECT_TRUE (elevationSideView (Pos::fromCartesian (1.f, 0.f, 0.f)).behind);
  EXPECT_FALSE (
      elevationSideView (Pos::fromCartesian (-1.f, 0.f, 0.f)).behind);
}

// Straight up and straight down have no bearing at all. They must not come
// back as a jump to one edge -- a figure that ends at the ceiling would flick
// sideways on its last point.
TEST (ElevationSideView, APoleHasNoBearingAndIsDrawnInTheMiddle)
{
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, 0.f, 1.f)).across,
               0.f, epsilon);
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, 0.f, -1.f)).across,
               0.f, epsilon);
}

// A take is a couple of thousand ticks and the circle is a couple of
// centimetres. Sampled down, and evenly, so the shape survives rather than
// its first tenth.
TEST (ElevationSideView, ALongTakeIsSampledDownAcrossItsWholeLength)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < 2000; ++i)
    {
      auto const t = static_cast<float> (i) / 2000.f;
      auto const theta = t * juce::MathConstants<float>::pi;
      ticks.push_back (
          Pos::fromCartesian (std::sin (theta), 0.f, std::cos (theta)));
    }

  auto const drawn = elevationSideView (ticks, 96);

  EXPECT_LE (drawn.size (), 96u);
  EXPECT_GT (drawn.size (), 48u) << "sampled so thin the figure is gone";
  EXPECT_NEAR (drawn.front ().frac, 0.f, 0.01f);
  EXPECT_GT (drawn.back ().frac, 0.9f) << "it stopped short of the end";
}

// An invalid tick is a hole in the take, not a point at the origin. Drawing
// it would put a spike through the middle of the picture.
TEST (ElevationSideView, InvalidTicksAreDroppedRatherThanDrawn)
{
  std::vector<Pos> ticks{ Pos::fromCartesian (0.f, 0.f, 1.f), Pos::invalid,
                          Pos::fromCartesian (0.f, 0.f, -1.f) };

  EXPECT_EQ (elevationSideView (ticks, 96).size (), 2u);
}
