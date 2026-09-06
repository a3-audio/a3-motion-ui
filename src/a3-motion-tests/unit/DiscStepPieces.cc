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

#include <cmath>

#include <JuceHeader.h>

#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-ui/components/SphereProjection.hh>

using namespace a3;

namespace
{
constexpr float maxStep = 0.03f;
constexpr float maxSwing = 0.02f;
constexpr int maxPieces = 256;

int
pieces (float x1, float y1, float x2, float y2)
{
  return discStepPieces (x1, y1, x2, y2, maxStep, maxSwing, maxPieces);
}
}

// A step that is short and stays on one side of the disc needs no cutting at
// all -- most of a path is this, and cutting it would cost a frame nothing is
// gained by.
TEST (DiscStepPieces, AShortStepOutInTheOpenIsOnePiece)
{
  EXPECT_EQ (pieces (0.5f, 0.5f, 0.51f, 0.5f), 1);
  EXPECT_EQ (pieces (-0.8f, 0.2f, -0.79f, 0.21f), 1);
}

// A long one is cut by its length, the way it always was: a straight line in
// the disc is a curve on the sphere, and it can only bend if it has joints.
TEST (DiscStepPieces, ALongStepIsCutByItsLength)
{
  EXPECT_GE (pieces (0.f, 0.5f, 0.f, 0.8f), 10);
}

// The one the length cannot see. The disc's angle is the azimuth, so a step
// straight past the origin turns through half a revolution -- and it is short,
// so by length alone it was one piece and drawn as one straight line.
TEST (DiscStepPieces, AStepPastTheOriginIsCutByHowFarItSwings)
{
  auto const across = pieces (-0.01f, 0.f, 0.01f, 0.f);

  EXPECT_GT (across, 20)
      << "a step that turns the azimuth through half a revolution was drawn "
         "as one straight line";
  EXPECT_LE (across, maxPieces);
}

// Nothing asks for more than it is allowed, whatever it is handed.
TEST (DiscStepPieces, NothingCostsMoreThanTheCap)
{
  EXPECT_LE (pieces (-1.f, 0.f, 1.f, 0.0001f), maxPieces);
  EXPECT_GE (pieces (0.f, 0.f, 0.f, 0.f), 1);
}

namespace
{
/** The longest single step the drawn line takes, once a 2D step has been cut
 *  into the pieces discStepPieces() asks for and each piece projected. */
float
worstDrawnStep (float x1, float y1, float x2, float y2, float base)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 0.5f;
  params.elevationBase = base;

  auto const project = [&] (float x, float y) {
    return heightMap.mapTo3D (Pos::fromCartesian (x, y, 0.f), params);
  };

  auto const n = pieces (x1, y1, x2, y2);
  auto previous = project (x1, y1);
  float worst = 0.f;

  for (int i = 1; i <= n; ++i)
    {
      auto const t = static_cast<float> (i) / static_cast<float> (n);
      auto const at = project (x1 + (x2 - x1) * t, y1 + (y2 - y1) * t);
      worst = std::max (
          worst, std::sqrt (std::pow (at.x () - previous.x (), 2.f)
                            + std::pow (at.y () - previous.y (), 2.f)
                            + std::pow (at.z () - previous.z (), 2.f)));
      previous = at;
    }

  return worst;
}
}

// What it is for, measured where it shows: the drawn step. With the base on
// the pole the disc's origin is drawn in the middle of the sphere and nothing
// near it costs anything; move the base off the pole -- which is what sway
// does -- and it is drawn out towards the rim.
TEST (DiscStepPieces, APathPastTheOriginIsDrawnAsACurveNotAChord)
{
  // A tenth of the sphere's radius. Longer than that and the eye reads a
  // straight line rather than part of a curve.
  for (float base : { 0.f, 0.25f, 0.5f, 0.75f })
    EXPECT_LT (worstDrawnStep (-0.02f, 0.005f, 0.02f, 0.005f, base), 0.1f)
        << "base " << base;
}

// And the case no amount of cutting can reach. Straight through the origin
// the azimuth is not fast, it is undefined: the path arrives at one bearing
// and leaves at the opposite one, so every sample on one side is half a
// revolution from every sample on the other. Several shipped shapes do this
// -- Clover, Infinity, Rose 4-Petal. It is a discontinuity, and the renderer
// lifts the pen at it rather than drawing the chord; this test is here so
// that nobody tries to fix it by cutting more finely.
TEST (DiscStepPieces, StraightThroughTheOriginStaysAJumpHoweverFinelyItIsCut)
{
  EXPECT_LT (worstDrawnStep (-0.02f, 0.f, 0.02f, 0.f, 0.f), 0.1f)
      << "on the pole both bearings are the same point, so there is no jump";

  EXPECT_GT (worstDrawnStep (-0.02f, 0.f, 0.02f, 0.f, 0.5f), 1.f)
      << "off the pole this is a jump, and cutting cannot make it not one";
}
