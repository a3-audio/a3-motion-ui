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
#include <a3-motion-ui/components/fpv/FloorBodies.hh>
#include <a3-motion-ui/components/fpv/FpvFloor.hh>
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

namespace
{
Vec2
atAngle (float angle, float radius)
{
  return { radius * std::cos (angle), radius * std::sin (angle) };
}

std::vector<float>
bearings ()
{
  // The speakers stand on the diagonals, so those are among them.
  auto const quarter = juce::MathConstants<float>::halfPi / 2.f;
  return { 0.f, 0.3f, quarter, 2.f, 3.f * quarter, -1.2f, -quarter };
}

std::vector<Vec2>
beyondTheRim ()
{
  std::vector<Vec2> points;
  for (auto const radius : { 1.1f, 1.3f, floorReach })
    for (auto const angle : bearings ())
      points.push_back (atAngle (angle, radius));
  return points;
}

std::vector<SphereCamera>
topAndLeaned ()
{
  return { SphereCamera{}, camera (0.f, 1.1f), camera (0.6f, 0.f),
           camera (0.9f, 2.f), camera (1.3f, -0.7f) };
}

float
horizontalDistance (Pos const &a, Pos const &b)
{
  return std::hypot (a.x () - b.x (), a.y () - b.y ());
}
}

TEST (GroupsOnTheDanceFloor, AGroupCrossesTheRimWithoutAJump)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, SphereCamera{}, speakerFloorZ };
  constexpr float step = 1e-4f;
  for (auto const angle : bearings ())
    {
      auto const inside = floorPointInRoom (atAngle (angle, 1.f - step),
                                            FloorSurface::DanceFloor, view);
      auto const rim = floorPointInRoom (atAngle (angle, 1.f),
                                         FloorSurface::DanceFloor, view);
      auto const outside = floorPointInRoom (atAngle (angle, 1.f + step),
                                             FloorSurface::DanceFloor, view);
      EXPECT_LT (horizontalDistance (inside, rim), 10.f * step) << "bearing " << angle;
      EXPECT_LT (horizontalDistance (rim, outside), 10.f * step) << "bearing " << angle;
      EXPECT_NEAR (outside.z (), speakerFloorZ, tolerance);
    }
}

TEST (GroupsOnTheDanceFloor, AtTheFloorsReachAGroupStandsWhereTheRoomIs)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, camera (0.7f, 0.4f), speakerFloorZ };
  for (auto const angle : bearings ())
    {
      auto const at = atAngle (angle, floorReach);
      auto const room = floorPointInRoom (at, FloorSurface::DanceFloor, view);
      ASSERT_TRUE (room.isValid ());
      EXPECT_NEAR (room.x (), at.x, tolerance);
      EXPECT_NEAR (room.y (), at.y, tolerance);
      EXPECT_NEAR (room.z (), speakerFloorZ, tolerance);
    }
}

TEST (GroupsOnTheDanceFloor, BeyondTheRimAGroupWalksStraightOut)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, SphereCamera{}, speakerFloorZ };
  for (auto const angle : bearings ())
    {
      auto last = 0.f;
      for (auto radius = 1.f; radius <= floorReach + 1e-6f; radius += 0.05f)
        {
          auto const room = floorPointInRoom (atAngle (angle, radius),
                                              FloorSurface::DanceFloor, view);
          auto const out = std::hypot (room.x (), room.y ());
          EXPECT_GT (out, last) << "bearing " << angle << " radius " << radius;
          EXPECT_NEAR (std::remainder (std::atan2 (room.y (), room.x ()) - angle,
                                       juce::MathConstants<float>::twoPi),
                       0.f, tolerance)
              << "its own bearing";
          last = out;
        }
    }
}

TEST (GroupsOnTheDanceFloor, BeyondTheRimAFloorPointComesBackFromTheScreen)
{
  HeightMapSphere heightMap;
  for (auto const &cam : topAndLeaned ())
    for (auto const at : beyondTheRim ())
      {
        FloorView const view{ heightMap, cam, speakerFloorZ };
        auto const onView = floorPointOnView (at, FloorSurface::DanceFloor, view);
        ASSERT_TRUE (onView.has_value ());
        auto const back = floorPointUnder (*onView, view);
        ASSERT_TRUE (back.has_value ())
            << "pitch " << cam.pitch << " turn " << cam.turn;
        EXPECT_NEAR (back->x, at.x, tolerance)
            << "pitch " << cam.pitch << " turn " << cam.turn << " at " << at.x << ", " << at.y;
        EXPECT_NEAR (back->y, at.y, tolerance)
            << "pitch " << cam.pitch << " turn " << cam.turn << " at " << at.x << ", " << at.y;
        // Exactly on the edge the round trip may land a hair outside it.
        if (at.getDistanceFromOrigin () < floorReach - tolerance)
          EXPECT_TRUE (onTheFloor (back));
      }
}

TEST (GroupsOnTheDanceFloor, FromStraightAboveTheFloorReachesTheSpeakers)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, SphereCamera{}, speakerFloorZ };
  for (auto const angle : bearings ())
    {
      auto const onView = floorPointOnView (atAngle (angle, floorReach),
                                            FloorSurface::DanceFloor, view);
      ASSERT_TRUE (onView.has_value ());
      EXPECT_NEAR (onView->getDistanceFromOrigin (), floorReach, tolerance);
    }
}

TEST (GroupsOnTheDanceFloor, AFingerBetweenTheRimAndTheSpeakersPlacesAGroup)
{
  HeightMapSphere heightMap;
  for (auto const &cam : topAndLeaned ())
    {
      FloorView const view{ heightMap, cam, speakerFloorZ };
      Vec2 const at{ 0.f, -1.2f };
      auto const onView = floorPointOnView (at, FloorSurface::DanceFloor, view);
      ASSERT_TRUE (onView.has_value ());
      auto const floor = floorPointUnder (*onView, view);
      ASSERT_EQ (fpvFingerDown (true, onTheFloor (floor), false), FpvFingerDown::Floor)
          << "pitch " << cam.pitch << " turn " << cam.turn;

      FloorBodies bodies;
      auto const id = bodies.add (*floor);
      ASSERT_TRUE (id.has_value ());
      EXPECT_NEAR (bodies.at (*id).x, at.x, tolerance);
      EXPECT_NEAR (bodies.at (*id).y, at.y, tolerance);
    }
}

TEST (GroupsOnTheDanceFloor, AFingerPastTheFloorsReachIsTheCamera)
{
  HeightMapSphere heightMap;
  for (auto const &cam : topAndLeaned ())
    {
      FloorView const view{ heightMap, cam, speakerFloorZ };
      auto const onView = floorPointOnView (atAngle (0.4f, floorReach + 0.15f),
                                            FloorSurface::DanceFloor, view);
      ASSERT_TRUE (onView.has_value ());
      auto const floor = floorPointUnder (*onView, view);
      ASSERT_TRUE (floor.has_value ()) << "the plane goes on; the floor does not";
      EXPECT_GT (floor->getDistanceFromOrigin (), floorReach);
      EXPECT_EQ (fpvFingerDown (true, onTheFloor (floor), false), FpvFingerDown::Camera)
          << "pitch " << cam.pitch << " turn " << cam.turn;
    }
}

TEST (GroupsOnTheDanceFloor, ADragPastTheFloorsReachHoldsTheGroupAtIt)
{
  HeightMapSphere heightMap;
  FloorView const view{ heightMap, SphereCamera{}, speakerFloorZ };
  FloorBodies bodies;
  auto const id = *bodies.add ({ 0.5f, 0.f });

  // Straight down the screen's y, which is the room's -x (cartesian2DHOA2JUCE).
  auto const far = floorPointUnder ({ 0.f, 3.f }, view);
  ASSERT_TRUE (far.has_value ());
  bodies.move (id, *far);
  EXPECT_NEAR (bodies.at (id).getDistanceFromOrigin (), floorReach, tolerance);
  auto const bearing = std::atan2 (far->y, far->x);
  EXPECT_NEAR (std::atan2 (bodies.at (id).y, bodies.at (id).x), bearing, tolerance)
      << "held where the finger points";
}

namespace
{
constexpr int side = 300;

/** Inside the rim and out to the speakers. */
std::vector<Vec2>
paintedPoints ()
{
  auto points = floorPoints ();
  points.push_back ({ 1.25f, 0.2f });
  points.push_back ({ -0.9f, -0.9f });
  return points;
}

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
    for (auto const at : paintedPoints ())
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

  for (auto const at : paintedPoints ())
    {
      auto const centre
          = floorPointOnView (at, FloorSurface::DanceFloor, view)->transformedBy (toPixels);
      auto const around = juce::Rectangle<float> (blob, blob).withCentre (centre).toNearestInt ();
      EXPECT_GT (pixelsAwayFromBackground (image, around), 0)
          << "a group at " << at.x << ", " << at.y << " is on the picture";
    }

  juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}
