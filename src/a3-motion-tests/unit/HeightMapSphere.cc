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
