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

#include <a3-motion-ui/components/SpeakerLightScaling.hh>
#include <a3-motion-ui/components/fpv/BodyLook.hh>
#include <a3-motion-ui/components/fpv/FlightScene.hh>

using namespace a3;

namespace
{
constexpr float tolerance = 1e-4f;

SphereCamera
camera (float pitch, float turn)
{
  SphereCamera c;
  c.pitch = pitch;
  c.turn = turn;
  return c;
}

void
expectNear (Vec3 a, Vec3 b, float within = tolerance)
{
  EXPECT_NEAR (a.x, b.x, within);
  EXPECT_NEAR (a.y, b.y, within);
  EXPECT_NEAR (a.z, b.z, within);
}

Vec3
seen (Pos const &room, SphereCamera const &c)
{
  return toVec3 (asSeenFrom (room, c));
}
}

// ── groups ──────────────────────────────────────────────────────────────

TEST (FlightScene, AMarkTheDiscsSizeIsTheDiscs)
{
  FlightTuning const t;
  for (auto mass : { t.groupMass, t.crowdMass, t.hotspotMass, t.deadZoneMass })
    EXPECT_FLOAT_EQ (discMarkRadius (mass, 0.1f, t), bodyRadius (mass, 0.1f, t));
}

TEST (FlightScene, AMarkSwellsOnTheOneAsTheDiscDid)
{
  EXPECT_FLOAT_EQ (swollenMarkRadius (0.1f, 1.f), 0.1f);
  EXPECT_FLOAT_EQ (swollenMarkRadius (0.1f, 1.6f), 0.1f * bodyPulseScale (1.6f));
}

TEST (FlightScene, AMarkLiesOnTheFloor)
{
  auto const feet = Pos::fromCartesian (0.3f, -0.2f, speakerFloorZ);
  for (auto const c : { camera (0.f, 0.f), camera (0.7f, 1.2f) })
    {
      auto const mark = floorMarkInScene (feet, 0.08f, BodyRole::Attract, c);
      EXPECT_FLOAT_EQ (mark.radius, 0.08f);
      EXPECT_FALSE (mark.deadZone);
      expectNear (mark.centre, seen (feet, c));
    }
  EXPECT_TRUE (floorMarkInScene (feet, 0.08f, BodyRole::Repel, camera (0.f, 0.f))
                   .deadZone);
}

TEST (FlightScene, AFootprintSeenFromAboveIsItsOwnSize)
{
  auto const feet = Pos::fromCartesian (0.2f, 0.4f, speakerFloorZ);
  EXPECT_NEAR (footprintRadiusOnView (feet, 0.1f, camera (0.f, 0.f)), 0.1f,
               tolerance);
}

TEST (FlightScene, ALeanedFootprintIsForeshortenedOneWay)
{
  auto const feet = Pos::fromCartesian (0.2f, 0.4f, speakerFloorZ);
  auto const pitch = 0.8f;
  EXPECT_NEAR (footprintRadiusOnView (feet, 0.1f, camera (pitch, 0.5f)),
               0.1f * (1.f + std::cos (pitch)) / 2.f, 1e-3f);
}

// ── ships ───────────────────────────────────────────────────────────────

TEST (FlightScene, AShipThatHasNotMovedHasNoCourse)
{
  ShipCourse course;
  EXPECT_FALSE (course.forward ().has_value ());
  course.update (Pos::fromCartesian (0.f, 0.f, 1.f), 0.01f);
  EXPECT_FALSE (course.forward ().has_value ());
}

TEST (FlightScene, AShipPointsWhereItWentAndKeepsItWhenItStops)
{
  ShipCourse course;
  course.update (Pos::fromCartesian (0.f, 0.f, 1.f), 0.01f);
  course.update (Pos::fromCartesian (0.f, 0.1f, 0.995f), 0.01f);
  ASSERT_TRUE (course.forward ().has_value ());
  EXPECT_GT (course.forward ()->y, 0.99f);

  // A shiver below the step is not a turn.
  course.update (Pos::fromCartesian (0.002f, 0.1f, 0.995f), 0.01f);
  EXPECT_GT (course.forward ()->y, 0.99f);

  // Gone and back somewhere else: the jump is not a course either.
  course.lose ();
  course.update (Pos::fromCartesian (1.f, 0.f, 0.f), 0.01f);
  EXPECT_GT (course.forward ()->y, 0.99f);
}

TEST (FlightScene, AShipFliesOverTheBallAlongItsCourse)
{
  auto const length = 0.1f;
  auto const ship = shipInScene (Pos::fromCartesian (0.f, 0.f, 1.f),
                                 Vec3{ 1.f, 0.f, 0.3f }, length,
                                 camera (0.f, 0.f));
  expectNear (ship.nose, { 1.f, 0.f, 0.f });
  expectNear (ship.up, { 0.f, 0.f, 1.f });
  expectNear (ship.centre, { 0.f, 0.f, 1.f + shipHoverOfLength * length });
  EXPECT_FLOAT_EQ (ship.length, length);
  EXPECT_FLOAT_EQ (ship.shade, 1.f);
}

TEST (FlightScene, AShipTurnsWithTheCamera)
{
  auto const c = camera (0.6f, 0.9f);
  auto const at = Pos::fromCartesian (0.6f, 0.f, 0.8f);
  auto const ship = shipInScene (at, Vec3{ 0.f, 1.f, 0.f }, 0.1f, c);
  expectNear (ship.nose, seen (Pos::fromCartesian (0.f, 1.f, 0.f), c));
  expectNear (ship.up, seen (at, c));
}

TEST (FlightScene, AShipWithNoCourseStillPointsAlongTheBall)
{
  for (auto const at : { Pos::fromCartesian (0.f, 0.f, 1.f),
                         Pos::fromCartesian (0.6f, 0.f, 0.8f),
                         Pos::fromCartesian (0.f, 0.f, -1.f) })
    {
      auto const ship = shipInScene (at, std::nullopt, 0.1f, camera (0.f, 0.f));
      EXPECT_NEAR (length (ship.nose), 1.f, tolerance);
      EXPECT_NEAR (dot (ship.nose, ship.up), 0.f, tolerance);
    }
}

TEST (FlightScene, TheFarHalfIsTheBackSide)
{
  EXPECT_FALSE (onTheBackSide ({ 0.f, 0.f, 0.3f }));
  EXPECT_TRUE (onTheBackSide ({ 0.f, 0.f, -0.3f }));
}

TEST (FlightScene, OnTheBackSideAShipIsSmallerAndDarker)
{
  auto const front = shipDepthCue (0.5f);
  EXPECT_FLOAT_EQ (front.scale, 1.f);
  EXPECT_FLOAT_EQ (front.shade, 1.f);
  auto const back = shipDepthCue (-0.5f);
  EXPECT_FLOAT_EQ (back.scale, shipBackScale);
  EXPECT_FLOAT_EQ (back.shade, shipBackShade);

  auto previous = shipDepthCue (shipBackSideBand);
  for (auto z = shipBackSideBand; z >= -shipBackSideBand; z -= 0.01f)
    {
      auto const cue = shipDepthCue (z);
      EXPECT_LE (cue.scale, previous.scale);
      EXPECT_LE (cue.shade, previous.shade);
      previous = cue;
    }
}

TEST (FlightScene, AShipOnTheFarSideIsDrawnWithTheCue)
{
  // Seen from straight above, the bottom of the ball is the far side.
  auto const ship = shipInScene (Pos::fromCartesian (0.f, 0.f, -1.f),
                                 Vec3{ 1.f, 0.f, 0.f }, 0.1f, camera (0.f, 0.f));
  EXPECT_FLOAT_EQ (ship.length, 0.1f * shipBackScale);
  EXPECT_FLOAT_EQ (ship.shade, shipBackShade);
  EXPECT_TRUE (hiddenByTheBall (ship.centre));
}

// ── what hides what ─────────────────────────────────────────────────────

TEST (FlightScene, TheBallHidesWhatIsBehindIt)
{
  EXPECT_TRUE (hiddenByTheBall ({ 0.f, 0.f, -1.2f }));
  EXPECT_TRUE (hiddenByTheBall ({ 0.5f, 0.f, -0.9f }));
  EXPECT_FALSE (hiddenByTheBall ({ 0.f, 0.f, 1.2f })) << "in front";
  EXPECT_FALSE (hiddenByTheBall ({ 1.5f, 0.f, -0.1f })) << "beside it";
  EXPECT_FALSE (hiddenByTheBall ({ 0.f, 0.f, -0.5f }))
      << "inside the glass, as a group on the floor under the listener is";
}

TEST (FlightScene, AMarkUnderTheListenerIsNotHiddenFromAbove)
{
  auto const mark = floorMarkInScene (Pos::fromCartesian (0.2f, 0.f, speakerFloorZ),
                                      0.08f, BodyRole::Attract, camera (0.f, 0.f));
  EXPECT_FALSE (hiddenByTheBall (mark.centre));
}

TEST (FlightScene, AMarkBeyondTheBallIsHiddenFromTheHorizon)
{
  // Looking in from the horizon, a mark past the rim on the far side lies
  // behind the ball.
  auto const c = camera (1.5f, 0.f);
  auto const near = floorMarkInScene (Pos::fromCartesian (-1.2f, 0.f, speakerFloorZ),
                                      0.08f, BodyRole::Attract, c);
  auto const far = floorMarkInScene (Pos::fromCartesian (1.2f, 0.f, speakerFloorZ),
                                     0.08f, BodyRole::Attract, c);
  EXPECT_NE (hiddenByTheBall (near.centre), hiddenByTheBall (far.centre))
      << "one of the two lies behind the ball";
}

// ── into the shader ─────────────────────────────────────────────────────

TEST (FlightScene, TheShaderSeesASeenPointAtMinusYX)
{
  auto const at = onShaderScreen ({ 0.3f, 0.5f, 0.9f });
  EXPECT_FLOAT_EQ (at[0], -0.5f);
  EXPECT_FLOAT_EQ (at[1], 0.3f);
}

TEST (FlightScene, NothingPackedIsAnEmptyScene)
{
  auto const packed = packFlightScene ({}, 0, {}, 0);
  EXPECT_GT (packed.bounds[0], packed.bounds[2]);
  EXPECT_GT (packed.bounds[1], packed.bounds[3]);
  for (auto v : packed.shipAt)
    EXPECT_FLOAT_EQ (v, 0.f);
  for (auto v : packed.markAt)
    EXPECT_FLOAT_EQ (v, 0.f);
}

TEST (FlightScene, AShipIsPackedFourToAnEntry)
{
  std::array<ShipInScene, maxSceneShips> ships{};
  auto &ship = ships[1];
  ship.centre = { 0.1f, 0.2f, 0.9f };
  ship.nose = { 1.f, 0.f, 0.f };
  ship.up = { 0.f, 0.f, 1.f };
  ship.length = 0.2f;
  ship.shade = 0.5f;
  ship.r = 0.25f;
  ship.g = 0.5f;
  ship.b = 0.75f;

  auto const packed = packFlightScene (ships, 2, {}, 0);
  EXPECT_FLOAT_EQ (packed.shipAt[0 + 3], 0.f) << "the first one is empty";
  EXPECT_FLOAT_EQ (packed.shipAt[4 + 0], 0.1f);
  EXPECT_FLOAT_EQ (packed.shipAt[4 + 1], 0.2f);
  EXPECT_FLOAT_EQ (packed.shipAt[4 + 2], 0.9f);
  EXPECT_FLOAT_EQ (packed.shipAt[4 + 3], 0.1f) << "half its length";
  EXPECT_FLOAT_EQ (packed.shipNose[4 + 0], 1.f);
  EXPECT_FLOAT_EQ (packed.shipNose[4 + 3], 0.5f) << "its shade";
  EXPECT_FLOAT_EQ (packed.shipUp[4 + 2], 1.f);
  EXPECT_GE (packed.shipUp[4 + 3], 0.1f) << "it reaches at least its half length";
  EXPECT_FLOAT_EQ (packed.shipColour[4 + 0], 0.25f);
  EXPECT_FLOAT_EQ (packed.shipColour[4 + 2], 0.75f);

  // On the screen at (-0.2, 0.1), and the box holds all of it.
  auto const reach = packed.shipUp[4 + 3];
  EXPECT_LE (packed.bounds[0], -0.2f - reach);
  EXPECT_LE (packed.bounds[1], 0.1f - reach);
  EXPECT_GE (packed.bounds[2], -0.2f + reach);
  EXPECT_GE (packed.bounds[3], 0.1f + reach);
}

TEST (FlightScene, AMarkIsPackedWithItsReach)
{
  std::array<FloorMark, maxSceneMarks> marks{};
  marks[0] = { { -0.4f, 0.3f, -0.1f }, 0.08f, false };
  marks[1] = { { 0.2f, 0.1f, -0.1f }, 0.1f, true };
  auto const packed = packFlightScene ({}, 0, marks, 2);
  EXPECT_FLOAT_EQ (packed.markAt[0], -0.4f);
  EXPECT_FLOAT_EQ (packed.markAt[3], 0.08f) << "its radius";
  EXPECT_FLOAT_EQ (packed.markShape[0], 0.f);
  EXPECT_FLOAT_EQ (packed.markShape[4], 1.f) << "a dead zone";
  EXPECT_GE (packed.markShape[1], 0.08f) << "reaches its radius on the screen";
  EXPECT_FLOAT_EQ (packed.markAt[8 + 3], 0.f) << "the rest are empty";

  auto const reach = packed.markShape[1];
  EXPECT_LE (packed.bounds[0], -0.3f - reach);
  EXPECT_GE (packed.bounds[3], -0.4f + reach);
}

TEST (FlightScene, AMarksRingsArePackedAndReached)
{
  std::array<FloorMark, maxSceneMarks> marks{};
  marks[0] = { { 0.f, 0.f, -0.1f }, 0.08f, false, 0.3f, 0.5f };
  auto const packed = packFlightScene ({}, 0, marks, 1);
  EXPECT_FLOAT_EQ (packed.markRing[0], 0.3f) << "the ring's radius";
  EXPECT_FLOAT_EQ (packed.markRing[1], 0.5f) << "how far the hold has come";
  EXPECT_GE (packed.markShape[1], 0.3f) << "the ring lies inside the reach";

  marks[0].ring = 0.f;
  marks[0].hold = 0.5f;
  EXPECT_GE (packFlightScene ({}, 0, marks, 1).markShape[1],
             0.08f * markHoldRingOfRadius)
      << "and so does the hold ring";
}

TEST (FlightScene, AMarkOfNoSizeIsNotPacked)
{
  // Before the first layout the blob, and so a mark, has no size; the
  // shader's hatch would step by nothing across it.
  std::array<FloorMark, maxSceneMarks> marks{};
  marks[0] = { { 0.f, 0.f, -0.1f }, 0.f, true, 0.3f, 0.f };
  auto const packed = packFlightScene ({}, 0, marks, 1);
  EXPECT_FLOAT_EQ (packed.markAt[3], 0.f);
  EXPECT_GT (packed.bounds[0], packed.bounds[2]);
}

TEST (FlightScene, MoreThanThereIsRoomForIsNotPacked)
{
  std::array<ShipInScene, maxSceneShips> ships{};
  for (auto &s : ships)
    s.length = 0.1f;
  auto const packed = packFlightScene (ships, 9, {}, -3);
  for (auto i = 0; i < maxSceneShips; ++i)
    EXPECT_FLOAT_EQ (packed.shipAt[static_cast<size_t> (4 * i + 3)], 0.05f);
}

// ── what the 2D pass keeps ──────────────────────────────────────────────

TEST (FlightScene, AHiddenThingsLabelDimsLikeItsGhost)
{
  EXPECT_FLOAT_EQ (labelAlpha (false), 1.f);
  EXPECT_LT (labelAlpha (true), 0.6f);
  EXPECT_GT (labelAlpha (true), 0.f) << "dimmed, never gone";
}

TEST (FlightScene, AMarkIsHitWhereItLies)
{
  auto const fingertip = 34.f;
  EXPECT_FLOAT_EQ (groupHitRadius (60.f, fingertip), 60.f)
      << "a mark bigger than a fingertip is hit on its footprint";
  EXPECT_FLOAT_EQ (groupHitRadius (5.f, fingertip), fingertip / 2.f);
}
