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

// With the sphere above overhead, this is the side view it has always been:
// the ceiling at the top of the circle, the floor at the bottom, ear height
// across the middle.
TEST (ElevationSideView, OverheadItIsTheSideView)
{
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, 0.f, 1.f)).down,
               -1.f, epsilon);
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (0.f, 0.f, -1.f)).down,
               1.f, epsilon);
  EXPECT_NEAR (elevationSideView (Pos::fromCartesian (1.f, 0.f, 0.f)).down,
               0.f, epsilon);
}

// And lean the sphere to the horizon and this comes up to overhead: the
// picture that loses the height is never the only one you have.
TEST (ElevationSideView, WithTheSphereOnItsSideThisIsTheOverheadView)
{
  SphereCamera const onItsSide{ juce::MathConstants<float>::halfPi, 0.f };

  // Straight up is now the middle of the circle rather than the top of it,
  // which is what looking down on a room from above does with the ceiling.
  auto const zenith
      = elevationSideView (Pos::fromCartesian (0.f, 0.f, 1.f), onItsSide);
  EXPECT_NEAR (zenith.across, 0.f, epsilon);
  EXPECT_NEAR (zenith.down, 0.f, epsilon);

  // And the front of the room is at the top of it.
  auto const front
      = elevationSideView (Pos::fromCartesian (1.f, 0.f, 0.f), onItsSide);
  EXPECT_NEAR (front.down, -1.f, epsilon);
}

// A finger in the circle points at something, and it is the same something
// the projection would have put under it -- a recording is set by dragging in
// here, so a mismatch moves the value away from the finger.
TEST (ElevationSideView, TheProjectionAndItsInverseAgree)
{
  for (auto const &camera : { SphereCamera{}, SphereCamera{ 0.7f, 1.2f },
                              SphereCamera{ -1.1f, -0.4f } })
    for (auto const &rough : { Pos::fromCartesian (0.f, 0.f, 1.f),
                               Pos::fromCartesian (0.6f, -0.5f, 0.62f),
                               Pos::fromCartesian (-0.3f, 0.8f, 0.52f) })
      {
        // Unit length, because that is what a direction is and what comes
        // back: the inverse hands over a point on the sphere, so a fixture
        // half a thousandth off it would be measuring the fixture.
        auto const length = std::sqrt (rough.x () * rough.x ()
                                       + rough.y () * rough.y ()
                                       + rough.z () * rough.z ());
        auto const at = Pos::fromCartesian (rough.x () / length,
                                            rough.y () / length,
                                            rough.z () / length);

        auto const drawn = elevationSideView (at, camera);
        if (drawn.behind)
          continue; // the far half is not what a finger can point at

        auto const back
            = elevationSideDirection (drawn.across, drawn.down, camera);

        EXPECT_NEAR (back.x (), at.x (), 1e-4f);
        EXPECT_NEAR (back.y (), at.y (), 1e-4f);
        EXPECT_NEAR (back.z (), at.z (), 1e-4f);
      }
}

// A finger past the rim is held at the rim rather than dropped: sliding off
// the edge should keep setting a value, not stop dead.
TEST (ElevationSideView, AFingerPastTheRimIsHeldAtIt)
{
  auto const out = elevationSideDirection (3.f, 0.f);

  EXPECT_NEAR (std::sqrt (out.x () * out.x () + out.y () * out.y ()
                          + out.z () * out.z ()),
               1.f, 1e-4f);
  EXPECT_NEAR (out.z (), 0.f, 1e-4f) << "held on the rim, which is the horizon";
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
  EXPECT_NEAR (drawn.front ().down, -1.f, 0.01f);
  EXPECT_GT (drawn.back ().down, 0.9f) << "it stopped short of the end";
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
// times a lap, and the pen has to lift at each one.
TEST (ElevationSideView, ACloverTearsFourTimesAndTheStrokesAreLifted)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 0.5f;
  params.elevationBase = 0.13f;

  auto const clover = [&] (float base) {
    params.elevationBase = base;
    std::vector<Pos> onSphere;
    for (int i = 0; i < 1024; ++i)
      {
        auto const phi = juce::MathConstants<float>::twoPi
                         * static_cast<float> (i) / 1024.f;
        auto const radius = std::cos (2.f * phi);
        onSphere.push_back (heightMap.mapTo3D (
            Pos::fromCartesian (radius * std::cos (phi),
                                radius * std::sin (phi), 0.f),
            params));
      }

    auto const drawn = elevationSideView (onSphere, 96);
    int breaks = 0;
    for (size_t i = 1; i < drawn.size (); ++i)
      if (drawn[i].startsStroke)
        ++breaks;
    return breaks;
  };

  EXPECT_EQ (clover (0.13f), 4) << "one per petal, and each one a lifted pen";
  EXPECT_EQ (clover (0.f), 0) << "at the pole there is nothing to lift for";
}
