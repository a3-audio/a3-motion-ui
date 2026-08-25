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
