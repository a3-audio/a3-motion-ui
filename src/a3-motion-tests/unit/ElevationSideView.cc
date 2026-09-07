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

#include <a3-motion-engine/elevation/HeightMapSphere.hh>
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

// The mapping tears at the pad's centre once the base is off the pole (see
// HeightMapSphere::mapTo3D): the figure leaves on the bearing opposite the one
// it arrived on. On the sphere that is a long jump and the pen lifts. In this
// picture it is *short* -- near the ceiling the circle is narrow, so the two
// ends of the tear are a few pixels apart -- and a pen that only watched the
// drawn distance kept writing, which is the straight line across the elevation
// circle.
//
// So the break is decided where the truth is, in the room: a step many times
// the size of the figure's usual step is not a step, it is a seam.
TEST (ElevationSideView, ASeamInTheRoomLiftsThePenInThePicture)
{
  std::vector<Pos> ticks;

  // A quarter of a latitude circle, walked evenly...
  auto const walk = [&ticks] (float fromDeg, float toDeg) {
    for (int i = 0; i <= 20; ++i)
      {
        auto const t = fromDeg + (toDeg - fromDeg) * static_cast<float> (i) / 20.f;
        auto const rad = t * juce::MathConstants<float>::pi / 180.f;
        ticks.push_back (Pos::fromCartesian (0.5f * std::cos (rad),
                                            0.5f * std::sin (rad), 0.866f));
      }
  };

  walk (0.f, 90.f);   // ... and then straight to the other side of the room.
  walk (250.f, 340.f);

  auto const drawn = elevationSideView (ticks, 96);

  ASSERT_EQ (drawn.size (), ticks.size ());
  EXPECT_TRUE (drawn.front ().startsStroke) << "the first point starts one";

  int breaks = 0;
  for (size_t i = 1; i < drawn.size (); ++i)
    if (drawn[i].startsStroke)
      {
        ++breaks;
        EXPECT_EQ (i, 21u) << "the pen lifted somewhere the figure is smooth";
      }

  EXPECT_EQ (breaks, 1) << "the seam was drawn as a line across the picture";
}

// And a figure with no seam in it is one unbroken stroke. A rule that lifted
// the pen on the largest step of every take would leave every figure with a
// gap in it, which is the fault it exists to prevent.
TEST (ElevationSideView, AFigureWithoutASeamIsDrawnInOnePiece)
{
  std::vector<Pos> ticks;
  for (int i = 0; i <= 200; ++i)
    {
      auto const rad
          = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 200.f;
      // A wobbling lap: the step size varies threefold along it.
      auto const z = 0.4f * std::sin (3.f * rad);
      auto const rXY = std::sqrt (1.f - z * z);
      ticks.push_back (
          Pos::fromCartesian (rXY * std::cos (rad), rXY * std::sin (rad), z));
    }

  auto const drawn = elevationSideView (ticks, 96);

  int breaks = 0;
  for (size_t i = 1; i < drawn.size (); ++i)
    if (drawn[i].startsStroke)
      ++breaks;

  EXPECT_EQ (breaks, 0);
}

// The case this was found on, end to end: a Clover, whose four petals all
// start and end at the pad's centre, with the base off the pole. It tears four
// times a lap -- and each tear is short in this picture, because near the
// ceiling the circle is only a few pixels wide, which is exactly why judging
// the break by drawn distance failed and drew a line across the circle.
TEST (ElevationSideView, ACloverTearsFourTimesAndTheStrokesAreLifted)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 0.5f;
  params.elevationBase = 0.13f; // where sway had left it when this was seen

  std::vector<Pos> onSphere;
  for (int i = 0; i < 1024; ++i)
    {
      auto const phi
          = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 1024.f;
      auto const radius = std::cos (2.f * phi); // through the origin, 4x a lap
      onSphere.push_back (heightMap.mapTo3D (
          Pos::fromCartesian (radius * std::cos (phi), radius * std::sin (phi),
                              0.f),
          params));
    }

  auto const drawn = elevationSideView (onSphere, 96);

  int breaks = 0;
  for (size_t i = 1; i < drawn.size (); ++i)
    if (drawn[i].startsStroke)
      ++breaks;

  EXPECT_EQ (breaks, 4) << "one per petal, and each one a lifted pen";

  // And at the pole there is nothing to lift: the same figure overhead is a
  // single unbroken stroke.
  params.elevationBase = 0.f;
  for (int i = 0; i < 1024; ++i)
    {
      auto const phi
          = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 1024.f;
      auto const radius = std::cos (2.f * phi);
      onSphere[static_cast<size_t> (i)] = heightMap.mapTo3D (
          Pos::fromCartesian (radius * std::cos (phi), radius * std::sin (phi),
                              0.f),
          params);
    }

  auto const overhead = elevationSideView (onSphere, 96);
  breaks = 0;
  for (size_t i = 1; i < overhead.size (); ++i)
    if (overhead[i].startsStroke)
      ++breaks;

  EXPECT_EQ (breaks, 0);
}

/** Turn the sphere above and this picture turns with it. Two pictures of one
 *  room that disagree about which way it is facing are worse than one picture,
 *  and the small one is the one that gets believed. */
TEST (ElevationSideView, ItFollowsTheSphereRoundTheRoom)
{
  auto const front = Pos::fromCartesian (1.f, 0.f, 0.f);

  // Straight ahead is straight ahead: dead centre, and behind the listener.
  EXPECT_NEAR (elevationSideView (front).across, 0.f, epsilon);

  // Walked a quarter turn round the room, the front of it is off to one side.
  auto const quarter = elevationSideView (
      front, juce::MathConstants<float>::halfPi);
  EXPECT_NEAR (std::abs (quarter.across), 1.f, epsilon);

  // And half a turn puts it dead centre again, on the near side this time.
  auto const half
      = elevationSideView (front, juce::MathConstants<float>::pi);
  EXPECT_NEAR (half.across, 0.f, 1e-3f);
  EXPECT_NE (half.behind, elevationSideView (front).behind);
}

/** The height is the height whichever way the room is turned -- walking round
 *  a sound does not raise it. */
TEST (ElevationSideView, TurningTheRoomDoesNotChangeAHeight)
{
  auto const up = Pos::fromCartesian (0.6f, 0.3f, 0.74f);

  for (float turn : { 0.f, 1.f, 2.5f, -2.f })
    EXPECT_NEAR (elevationSideView (up, turn).frac,
                 elevationSideView (up).frac, epsilon)
        << "turn " << turn;
}
