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
#include <a3-motion-ui/Helpers.hh>
#include <a3-motion-ui/components/LineMapGeometry.hh>

#include <algorithm>

using namespace a3;

namespace
{

// The line the shader lights has to be the line the blob runs on, whichever
// rasteriser draws it (a3-motion-ui#34). So every point is held against the
// engine's own chain -- shape, lift, turn to the camera -- rather than against
// numbers written down here.

juce::Point<float>
asTheEngineSeesIt (float x, float y, ElevationParams const &params,
                   HeightMap const &heightMap, PlaneShaping const &shaping,
                   SphereCamera const &camera)
{
  auto const lifted = heightMap.mapTo3D (
      shapedPosition (Pos::fromCartesian (x, y, 0.f), shaping), params);
  return cartesian2DHOA2JUCE (asSeenFrom (lifted, camera));
}

juce::Path
strokeFrom (float x1, float y1, float x2, float y2)
{
  juce::Path path;
  path.startNewSubPath (x1, y1);
  path.lineTo (x2, y2);
  return path;
}

int
runsIn (ProjectedLine const &line)
{
  return static_cast<int> (
      std::count (line.startsRun.begin (), line.startsRun.end (), true));
}

}

TEST (LineMapGeometry, AnEmptyPathProjectsToNothing)
{
  HeightMapSphere heightMap;
  auto const line = projectLine ({}, {}, heightMap, {}, {});
  EXPECT_TRUE (line.points.empty ());
}

TEST (LineMapGeometry, OneStrokeIsOneRunAndTheThreeListsAgree)
{
  HeightMapSphere heightMap;
  auto const line = projectLine (strokeFrom (0.1f, 0.2f, 0.4f, 0.2f), {},
                                 heightMap, {}, {});

  ASSERT_GE (line.points.size (), 2u);
  EXPECT_EQ (line.depth.size (), line.points.size ());
  EXPECT_EQ (line.startsRun.size (), line.points.size ());
  EXPECT_TRUE (line.startsRun.front ());
  EXPECT_EQ (runsIn (line), 1);
}

TEST (LineMapGeometry, TwoSubPathsAreTwoRuns)
{
  HeightMapSphere heightMap;
  auto path = strokeFrom (0.1f, 0.2f, 0.3f, 0.2f);
  path.startNewSubPath (-0.3f, -0.2f);
  path.lineTo (-0.1f, -0.2f);

  EXPECT_EQ (runsIn (projectLine (path, {}, heightMap, {}, {})), 2);
}

TEST (LineMapGeometry, BothEndsAreWhereTheEngineWouldPutTheBlob)
{
  HeightMapSphere heightMap;
  ElevationParams params;
  params.reach = 0.7f;
  PlaneShaping shaping;
  shaping.turns = 0.125f;
  shaping.squeezeX = 0.3f;
  SphereCamera const camera{ 0.6f, -0.9f };

  auto const line = projectLine (strokeFrom (0.1f, 0.2f, 0.4f, -0.1f), params,
                                 heightMap, shaping, camera);
  ASSERT_GE (line.points.size (), 2u);

  auto const first
      = asTheEngineSeesIt (0.1f, 0.2f, params, heightMap, shaping, camera);
  auto const last
      = asTheEngineSeesIt (0.4f, -0.1f, params, heightMap, shaping, camera);
  EXPECT_NEAR (line.points.front ().x, first.x, 1e-5f);
  EXPECT_NEAR (line.points.front ().y, first.y, 1e-5f);
  EXPECT_NEAR (line.points.back ().x, last.x, 1e-5f);
  EXPECT_NEAR (line.points.back ().y, last.y, 1e-5f);
}

TEST (LineMapGeometry, DepthIsHowFarTowardsTheViewer)
{
  HeightMapSphere heightMap;
  SphereCamera const camera{ 0.6f, -0.9f };
  auto const line = projectLine (strokeFrom (0.1f, 0.2f, 0.4f, -0.1f), {},
                                 heightMap, {}, camera);
  ASSERT_FALSE (line.points.empty ());

  // ElevationParams{} spelled out, not `{}`: HeightMapSphere also has
  // mapTo3D (Pos, float coverage), and a bare `{}` picks that one with a
  // coverage of zero -- two hundredths of depth off, and a test that looked
  // like a fault in the code under test.
  auto const seen = asSeenFrom (
      heightMap.mapTo3D (shapedPosition (Pos::fromCartesian (0.1f, 0.2f, 0.f),
                                         PlaneShaping{}),
                         ElevationParams{}),
      camera);
  EXPECT_NEAR (line.depth.front (), seen.z (), 1e-5f);
}

TEST (LineMapGeometry, ALongStepIsCutSoTheLineFollowsTheSphere)
{
  // Straight across the disc in one step: drawn as one chord it would cut
  // through the sphere instead of running over it.
  HeightMapSphere heightMap;
  auto const line = projectLine (strokeFrom (-0.8f, 0.f, 0.8f, 0.f), {},
                                 heightMap, {}, {});
  EXPECT_GT (line.points.size (), 10u);
}
