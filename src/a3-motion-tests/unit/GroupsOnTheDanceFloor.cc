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

#include <cmath>
#include <vector>

#include <JuceHeader.h>

#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/SpeakerLightScaling.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/components/fpv/DanceFloor.hh>
#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

namespace
{
constexpr float tolerance = 1e-3f;

SphereCamera
camera (float pitch, float turn)
{
  SphereCamera c;
  c.pitch = pitch;
  c.turn = turn;
  return c;
}

std::vector<SphereCamera>
cameras ()
{
  return { camera (0.f, 0.f), camera (0.6f, 0.f), camera (0.9f, 2.f),
           camera (1.3f, -0.7f), camera (0.f, 1.1f) };
}

std::vector<Vec2>
floorPoints ()
{
  return { { 0.f, 0.f },   { 0.5f, 0.2f }, { -0.3f, 0.7f },
           { 0.9f, -0.1f }, { 0.f, -1.f },  { -0.6f, -0.6f } };
}
}

TEST (GroupsOnTheDanceFloor, AGroupStandsStraightBelowItsSpherePoint)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, camera (0.7f, 0.4f), speakerFloorZ };
  for (auto const at : floorPoints ())
    {
      auto const ball = floorPointInRoom (at, FloorSurface::Sphere, view);
      auto const floor = floorPointInRoom (at, FloorSurface::DanceFloor, view);
      EXPECT_NEAR (floor.x (), ball.x (), tolerance);
      EXPECT_NEAR (floor.y (), ball.y (), tolerance);
      EXPECT_NEAR (floor.z (), speakerFloorZ, tolerance);
      EXPECT_GE (ball.z (), 0.f) << "the floor's points live on the upper half";
    }
}

TEST (GroupsOnTheDanceFloor, AFloorPointComesBackFromTheScreen)
{
  HeightMapSphere heightMap;
  for (auto const &cam : cameras ())
    for (auto const at : floorPoints ())
      {
        FloorView const view{ heightMap, cam, speakerFloorZ };
        auto const onView = floorPointOnView (at, FloorSurface::DanceFloor, view);
        ASSERT_TRUE (onView.has_value ());
        auto const back = floorPointUnder (*onView, view);
        ASSERT_TRUE (back.has_value ())
            << "pitch " << cam.pitch << " turn " << cam.turn;
        EXPECT_NEAR (back->x, at.x, tolerance)
            << "pitch " << cam.pitch << " turn " << cam.turn;
        EXPECT_NEAR (back->y, at.y, tolerance)
            << "pitch " << cam.pitch << " turn " << cam.turn;
      }
}

TEST (GroupsOnTheDanceFloor, FromStraightAboveAGroupIsWhereTheBallPutItBefore)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, SphereCamera{}, speakerFloorZ };
  for (auto const at : floorPoints ())
    {
      auto const floor = floorPointOnView (at, FloorSurface::DanceFloor, view);
      auto const ball = cartesian2DHOA2JUCE (
          heightMap.mapTo3D (Pos::fromCartesian (at.x, at.y, 0.f), ElevationParams{}));
      ASSERT_TRUE (floor.has_value ());
      EXPECT_NEAR (floor->x, ball.x, tolerance);
      EXPECT_NEAR (floor->y, ball.y, tolerance);
    }
}

TEST (GroupsOnTheDanceFloor, TheShipsPathStaysOnTheSphere)
{
  HeightMapSphere heightMap;
  auto const cam = camera (0.9f, 0.5f);
  FloorView const view{ heightMap, cam, speakerFloorZ };
  Vec2 const at{ 0.4f, -0.3f };

  auto const onSphere = floorPointOnView (at, FloorSurface::Sphere, view);
  auto const asBefore = cartesian2DHOA2JUCE (asSeenFrom (
      heightMap.mapTo3D (Pos::fromCartesian (at.x, at.y, 0.f), ElevationParams{}), cam));
  ASSERT_TRUE (onSphere.has_value ());
  EXPECT_NEAR (onSphere->x, asBefore.x, tolerance);
  EXPECT_NEAR (onSphere->y, asBefore.y, tolerance);

  auto const onFloor = floorPointOnView (at, FloorSurface::DanceFloor, view);
  ASSERT_TRUE (onFloor.has_value ());
  EXPECT_GT (onFloor->getDistanceFrom (*onSphere), 0.05f)
      << "leaned over, the floor is seen below the ball";
}

TEST (GroupsOnTheDanceFloor, LookingInFromTheHorizonFindsNoFloor)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, camera (juce::MathConstants<float>::halfPi, 0.f),
                        speakerFloorZ };
  EXPECT_FALSE (danceFloorUnder ({ 0.f, 0.f }, view).has_value ());
  EXPECT_FALSE (floorPointUnder ({ 0.2f, 0.5f }, view).has_value ());
}

TEST (GroupsOnTheDanceFloor, AFloorBehindTheEyeIsNotTouched)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, camera (1.5f, 0.f), speakerFloorZ };
  // Leaned this far, the floor seen below the picture would lie behind the
  // eye; the shader does not draw it there.
  EXPECT_FALSE (danceFloorUnder ({ 0.f, 1.5f }, view).has_value ());
  EXPECT_TRUE (danceFloorUnder ({ 0.f, 0.f }, view).has_value ());
}

TEST (GroupsOnTheDanceFloor, ARayMeetsTheFloorAtItsHeight)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, camera (1.f, -0.3f), speakerFloorZ };
  auto const hit = danceFloorUnder ({ 0.3f, -0.2f }, view);
  ASSERT_TRUE (hit.has_value ());
  EXPECT_NEAR (hit->z (), speakerFloorZ, tolerance);
}

TEST (GroupsOnTheDanceFloor, AFingerOffTheSphereIsHeldAtItsEdgeAsBefore)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, SphereCamera{}, speakerFloorZ };
  auto const far = floorPointUnder ({ 0.f, -3.f }, view);
  auto const edge = floorPointUnder ({ 0.f, -1.f }, view);
  ASSERT_TRUE (far.has_value ());
  ASSERT_TRUE (edge.has_value ());
  EXPECT_NEAR (far->x, edge->x, tolerance);
  EXPECT_NEAR (far->y, edge->y, tolerance);
  EXPECT_GT (far->getDistanceFromOrigin (), 1.f) << "off the room, so no group";
}

namespace
{
constexpr int side = 300;

int
pixelsAwayFromBackground (juce::Image const &image, juce::Rectangle<int> area)
{
  auto const bg = toColour (theme ().background);
  auto count = 0;
  for (int y = area.getY (); y < area.getBottom (); ++y)
    for (int x = area.getX (); x < area.getRight (); ++x)
      count += image.getPixelAt (x, y) != bg ? 1 : 0;
  return count;
}
}

TEST (GroupsOnTheDanceFloorPaint, GroupsPaintOnALeanedFloor)
{
  LookAndFeel_A3 lookAndFeel;
  juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

  HeightMapSphere heightMap;
  FloorView const view{ heightMap, camera (0.9f, 0.6f), speakerFloorZ };
  FlightTuning const tuning;
  // The sphere fills the middle half of the picture, as it would on screen.
  auto const toPixels = juce::AffineTransform::scale (side / 4.f)
                            .translated (side / 2.f, side / 2.f);
  auto const blob = side / 12.f;

  juce::Image image (juce::Image::ARGB, side, side, true);
  {
    juce::Graphics g (image);
    g.fillAll (toColour (theme ().background));
    auto label = 0;
    for (auto const at : floorPoints ())
      {
        auto const onView = floorPointOnView (at, FloorSurface::DanceFloor, view);
        ASSERT_TRUE (onView.has_value ());
        BodyPaint body;
        body.centre = onView->transformedBy (toPixels);
        body.radius = bodyRadius (tuning.crowdMass, blob, tuning);
        body.mass = tuning.crowdMass;
        body.label = bodyLabel (label++);
        body.pulse = 1.f;
        body.ringRadius = body.radius * 1.4f;
        body.stroke = theme ().strokeThin;
        body.fontHeight = body.radius * 0.5f;
        paintBody (g, body);
      }
  }

  for (auto const at : floorPoints ())
    {
      auto const centre
          = floorPointOnView (at, FloorSurface::DanceFloor, view)->transformedBy (toPixels);
      auto const around = juce::Rectangle<float> (blob, blob).withCentre (centre).toNearestInt ();
      EXPECT_GT (pixelsAwayFromBackground (image, around), 0)
          << "a group at " << at.x << ", " << at.y << " is on the picture";
    }

  juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
