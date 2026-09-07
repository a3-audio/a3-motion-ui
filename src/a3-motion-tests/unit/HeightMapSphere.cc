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

using namespace a3;

namespace
{
constexpr float coverage = 0.5f; // hemisphere, thetaMax = pi/2
constexpr float epsilon = 1e-4f;
}

TEST (HeightMapSphere, WrapContinuesUnderSphereBeyondEdge)
{
  HeightMapSphere heightMap;
  heightMap.setEdgeMode (HeightMapSphere::EdgeMode::Wrap);

  // Dragged far outside the visible disc (r = 3, well past r = 1).
  auto const pos3D = heightMap.mapTo3D (Pos::fromCartesian (3.f, 0.f, 0.f), coverage);

  // theta grows past thetaMax and saturates at pi -> south pole.
  EXPECT_NEAR (pos3D.z (), -1.f, epsilon);
}

TEST (HeightMapSphere, ClampFreezesAtEdgeInstead)
{
  HeightMapSphere heightMap;
  heightMap.setEdgeMode (HeightMapSphere::EdgeMode::Clamp);

  auto const pos3D = heightMap.mapTo3D (Pos::fromCartesian (3.f, 0.f, 0.f), coverage);

  // theta is pinned at thetaMax (equator for coverage = 0.5) regardless of r.
  EXPECT_NEAR (pos3D.z (), 0.f, epsilon);
  EXPECT_NEAR (pos3D.x (), 1.f, epsilon);
}

TEST (HeightMapSphere, EdgeModeDefaultsToWrap)
{
  HeightMapSphere heightMap;
  EXPECT_EQ (heightMap.getEdgeMode (), HeightMapSphere::EdgeMode::Wrap);
}

// Two ways of putting a channel position on screen live side by side in
// MotionComponent, and they are not the same projection:
//
//   - dropping z (cartesian2DHOA2JUCE) is the orthographic view — what you see
//   - the height map's mapTo2D works in the pattern coordinate system, which
//     reaches to sqrt(2) so a Square's corners fit (kPatternCoordinateMaxRadius)
//
// They differ by exactly that sqrt(2). Mixing them is what dragged every blob
// to the centre: disoccludeBlobs read a position by dropping z and wrote it
// back through setChannel2DPosition, which reads its input as height-map 2D.
TEST (HeightMapSphere, DroppingZIsNotTheHeightMapsOwnProjection)
{
  HeightMapSphere heightMap;
  ElevationParams params;

  auto const lifted
      = heightMap.mapTo3D (Pos::fromCartesian (0.8f, 0.f, 0.f), params);

  auto const droppedZ = std::hypot (lifted.x (), lifted.y ());
  auto const viaMap = heightMap.mapTo2D (lifted, params);

  EXPECT_NEAR (droppedZ * std::sqrt (2.f),
               std::hypot (viaMap.x (), viaMap.y ()), 0.001f);
}

// Which makes the round trip a shrink by 1/sqrt(2) per frame: ten frames of a
// drag took a blob from 0.57 to 0.02, i.e. onto the centre.
TEST (HeightMapSphere, DropZRoundTripShrinksTowardsTheCentre)
{
  HeightMapSphere heightMap;
  ElevationParams params;

  auto position
      = heightMap.mapTo3D (Pos::fromCartesian (0.8f, 0.f, 0.f), params);
  auto const startRadius = std::hypot (position.x (), position.y ());

  for (int frame = 0; frame < 10; ++frame)
    position = heightMap.mapTo3D (
        Pos::fromCartesian (position.x (), position.y (), 0.f), params);

  EXPECT_LT (std::hypot (position.x (), position.y ()), startRadius * 0.1f);
}

// The round trip disoccludeBlobs has to use instead. This one is the identity,
// which is the whole point: a blob nobody touched must not move.
TEST (HeightMapSphere, MapTo2DRoundTripLeavesAPositionWhereItWas)
{
  HeightMapSphere heightMap;
  ElevationParams params;

  auto position
      = heightMap.mapTo3D (Pos::fromCartesian (0.8f, 0.f, 0.f), params);
  auto const startRadius = std::hypot (position.x (), position.y ());

  for (int frame = 0; frame < 10; ++frame)
    position = heightMap.mapTo3D (heightMap.mapTo2D (position, params), params);

  EXPECT_NEAR (std::hypot (position.x (), position.y ()), startRadius, 0.001f);
}

// ── The elevation base ───────────────────────────────────────────────────

namespace
{
/** The colatitude a point comes back at, 0 at the north pole and 1 at the
 *  south, which is the fraction the mapping works in. */
float
fracOf (Pos const &pos3D)
{
  return std::acos (std::clamp (pos3D.z (), -1.f, 1.f)) / juce::MathConstants<float>::pi;
}

ElevationParams
baseParams (float base, float reach = 0.5f)
{
  ElevationParams params;
  params.reach = reach;
  params.elevationBase = base;
  return params;
}
}

// r = 0 -- the middle of the trajectory -- lands on the base, wherever it is
// put. That is the whole of what the line in the graphic sets.
TEST (HeightMapSphere, TheCentreLandsOnTheBase)
{
  HeightMapSphere heightMap;
  auto const centre = Pos::fromCartesian (0.f, 0.f, 0.f);

  for (float base : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
    EXPECT_NEAR (fracOf (heightMap.mapTo3D (centre, baseParams (base))), base,
                 epsilon)
        << "base " << base;
}

// A base of zero is exactly what the mapping did before there was one: the
// centre at the north pole and the reach cone growing downwards.
TEST (HeightMapSphere, ABaseOfZeroIsTheOldNorthPoleBehaviour)
{
  HeightMapSphere heightMap;

  ElevationParams before;
  before.reach = 0.5f;
  before.mirrorSouth = false;

  for (float x : { 0.f, 0.3f, 0.7f, 1.f, 1.41f })
    {
      auto const at = Pos::fromCartesian (x, 0.f, 0.f);
      EXPECT_NEAR (fracOf (heightMap.mapTo3D (at, baseParams (0.f))),
                   fracOf (heightMap.mapTo3D (at, before)), epsilon)
          << "at r " << x;
    }
}

// And a base of one is the mirror image of a base of zero -- the centre at
// the south pole and the cone growing up, which is what mirrorSouth used to
// mean. It cannot be checked against mirrorSouth itself, because nothing
// reads that flag any more; the equivalence that matters is held where the
// migration happens, in the ClipFile tests.
TEST (HeightMapSphere, ABaseOfOneMirrorsABaseOfZero)
{
  HeightMapSphere heightMap;

  for (float x : { 0.f, 0.3f, 0.7f, 1.f, 1.41f })
    {
      auto const at = Pos::fromCartesian (x, 0.f, 0.f);

      auto const fromNorth = fracOf (heightMap.mapTo3D (at, baseParams (0.f)));
      auto const fromSouth = fracOf (heightMap.mapTo3D (at, baseParams (1.f)));

      EXPECT_NEAR (fromSouth, 1.f - fromNorth, epsilon) << "at r " << x;
    }
}

// The trajectory grows towards whichever pole is further away, so reach
// always has room. Above the equator that is downwards, below it upwards.
TEST (HeightMapSphere, ItGrowsTowardsTheFurtherPole)
{
  HeightMapSphere heightMap;
  auto const edge = Pos::fromCartesian (1.41f, 0.f, 0.f);

  EXPECT_GT (fracOf (heightMap.mapTo3D (edge, baseParams (0.2f))), 0.2f)
      << "a base near the north pole must reach down";
  EXPECT_LT (fracOf (heightMap.mapTo3D (edge, baseParams (0.8f))), 0.8f)
      << "a base near the south pole must reach up";
}

// Right on the equator it has to pick one, and it picks the same one every
// time: south, which is the direction reach has always grown in.
TEST (HeightMapSphere, OnTheEquatorItPicksSouth)
{
  HeightMapSphere heightMap;
  auto const edge = Pos::fromCartesian (1.41f, 0.f, 0.f);

  EXPECT_GT (fracOf (heightMap.mapTo3D (edge, baseParams (0.5f))), 0.5f);
}

// mapTo2D is the exact inverse, and has to stay so with a base in play: a
// recording is written through it and played back through mapTo3D, so a
// mismatch would move every take the moment its base was touched.
TEST (HeightMapSphere, TheInverseStillComesBackWithABase)
{
  HeightMapSphere heightMap;

  for (float base : { 0.f, 0.3f, 0.5f, 0.9f, 1.f })
    for (float x : { 0.1f, 0.4f, 0.9f })
      {
        auto const params = baseParams (base);
        auto const there = Pos::fromCartesian (x, 0.2f, 0.f);

        auto const round
            = heightMap.mapTo2D (heightMap.mapTo3D (there, params), params);

        EXPECT_NEAR (round.x (), there.x (), 1e-3f)
            << "base " << base << " at " << x;
        EXPECT_NEAR (round.y (), there.y (), 1e-3f)
            << "base " << base << " at " << x;
      }
}

// ── The cone's direction, held across a sweep ────────────────────────────

// The rule mapTo3D uses on its own -- the cone grows towards whichever pole
// is further away -- is discontinuous at a base of exactly 0.5. Walking the
// base through it, a point of the figure jumped 1.36 on the unit sphere:
// seventeen times the step either side of it, and a jump in the sound, not
// only in the picture. sway sweeps the base straight through that point.
TEST (HeightMapSphere, TellingTheConeWhichWayToGrowMakesTheBaseContinuous)
{
  HeightMapSphere heightMap;

  auto const point = Pos::fromCartesian (1.f, 0.f, 0.f);
  auto const walk = [&] (ElevationParams::ConeDirection direction) {
    ElevationParams params;
    params.reach = 0.5f;
    params.coneDirection = direction;

    auto worst = 0.f;
    auto previous = Pos{};
    for (int i = 0; i <= 40; ++i)
      {
        params.elevationBase = static_cast<float> (i) / 40.f;
        auto const at = heightMap.mapTo3D (point, params);

        if (i > 0)
          worst = std::max (
              worst, std::sqrt (std::pow (at.x () - previous.x (), 2.f)
                                + std::pow (at.y () - previous.y (), 2.f)
                                + std::pow (at.z () - previous.z (), 2.f)));
        previous = at;
      }

    return worst;
  };

  // Left to work it out for itself, it turns the figure inside out halfway.
  EXPECT_GT (walk (ElevationParams::ConeDirection::FromBase), 1.f)
      << "the rule this replaces was continuous after all";

  // Told once, it is a smooth travel from one end to the other. A fortieth of
  // the range is a step of about 0.08; anything past a quarter is a jump.
  EXPECT_LT (walk (ElevationParams::ConeDirection::South), 0.25f);
  EXPECT_LT (walk (ElevationParams::ConeDirection::North), 0.25f);
}

// And what the direction actually means, so "South" cannot quietly become the
// other one: growing south puts the figure's outer edge below its base.
TEST (HeightMapSphere, TheConeGrowsTheWayItIsTold)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 0.5f;
  params.elevationBase = 0.5f;

  auto const edge = Pos::fromCartesian (1.41421356f, 0.f, 0.f);

  params.coneDirection = ElevationParams::ConeDirection::South;
  auto const south = heightMap.mapTo3D (edge, params);

  params.coneDirection = ElevationParams::ConeDirection::North;
  auto const north = heightMap.mapTo3D (edge, params);

  // z counts upwards, so further south is lower.
  EXPECT_LT (south.z (), north.z ());
}
