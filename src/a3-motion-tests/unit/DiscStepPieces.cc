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



// ── What is actually drawn, once the pieces are cut adaptively ───────────

namespace
{
/** The longest step the drawn line takes across one disc step, sampled the
 *  way the renderer samples it. */
float
worstDrawn (float x1, float y1, float x2, float y2, float reach, float base)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = reach;
  params.elevationBase = base;

  auto const project = [&] (float x, float y) {
    return heightMap.mapTo3D (Pos::fromCartesian (x, y, 0.f), params);
  };
  auto const apart = [] (Pos const &a, Pos const &b) {
    return std::sqrt (std::pow (a.x () - b.x (), 2.f)
                      + std::pow (a.y () - b.y (), 2.f)
                      + std::pow (a.z () - b.z (), 2.f));
  };

  auto previous = project (x1, y1);
  auto worst = 0.f;

  sampleDiscStep (x1, y1, x2, y2, DiscSampling{}, project, apart,
                  [&] (Pos const &point, bool joined) {
                    if (joined)
                      worst = std::max (worst, apart (previous, point));
                    previous = point;
                  });

  return worst;
}

/** How many times the sampler tells the drawer to lift the pen across this
 *  step: how many pieces it could not resolve. */
int
liftsAcross (float x1, float y1, float x2, float y2, float reach, float base)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = reach;
  params.elevationBase = base;

  auto const project = [&] (float x, float y) {
    return heightMap.mapTo3D (Pos::fromCartesian (x, y, 0.f), params);
  };
  auto const apart = [] (Pos const &a, Pos const &b) {
    return std::sqrt (std::pow (a.x () - b.x (), 2.f)
                      + std::pow (a.y () - b.y (), 2.f)
                      + std::pow (a.z () - b.z (), 2.f));
  };

  auto lifts = 0;
  sampleDiscStep (x1, y1, x2, y2, DiscSampling{}, project, apart,
                  [&lifts] (Pos const &, bool joined) {
                    if (!joined)
                      ++lifts;
                  });

  return lifts;
}

/** The longest piece the drawer is told it may *join*. Anything longer is a
 *  line across the sphere that is in none of the data. */
float
worstJoined (float x1, float y1, float x2, float y2, float reach, float base)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = reach;
  params.elevationBase = base;

  auto const project = [&] (float x, float y) {
    return heightMap.mapTo3D (Pos::fromCartesian (x, y, 0.f), params);
  };
  auto const apart = [] (Pos const &a, Pos const &b) {
    return std::sqrt (std::pow (a.x () - b.x (), 2.f)
                      + std::pow (a.y () - b.y (), 2.f)
                      + std::pow (a.z () - b.z (), 2.f));
  };

  auto previous = project (x1, y1);
  auto worst = 0.f;

  sampleDiscStep (x1, y1, x2, y2, DiscSampling{}, project, apart,
                  [&] (Pos const &point, bool joined) {
                    if (joined)
                      worst = std::max (worst, apart (previous, point));
                    previous = point;
                  });

  return worst;
}
}

// Cutting evenly could not do this. Measured before the change: a path
// missing the disc's origin by two thousandths, with a long reach and the
// base off the pole, still drew a third of the sphere in one straight line
// after a hundred and fifty even pieces -- the swing is concentrated at the
// closest approach, and even pieces spend themselves out where nothing is
// happening.
TEST (DiscStepPieces, APathThatOnlyJustMissesTheOriginIsStillACurve)
{
  struct
  {
    float reach, base, miss;
  } const cases[]{
    { 0.5f, 0.f, 0.005f },  { 0.5f, 0.25f, 0.005f },
    { 0.9f, 0.6f, 0.02f },  { 0.9f, 0.6f, 0.002f },
    { 1.0f, 0.5f, 0.01f },  { 0.9f, 0.6f, 0.0005f },
  };

  for (auto const &c : cases)
    EXPECT_LT (worstDrawn (-0.06f, c.miss, 0.06f, c.miss, c.reach, c.base),
               0.1f)
        << "reach " << c.reach << ", base " << c.base << ", missing by "
        << c.miss;
}

// Straight *through* it is the one case sampling cannot rescue, and it is
// worth being precise about why: with the figure held on the room's vertical
// axis, the pad's origin is a single point standing for a whole latitude
// circle whenever the base is off the pole. A path arrives at one bearing and
// leaves at the opposite one, and no number of pieces makes that shorter --
// halving the step halves nothing, because the two ends are on opposite sides
// of the room whatever the step.
//
// Overhead the origin is the pole and there is nothing to tear. Elsewhere the
// renderer lifts the pen at it (maxJump in drawPathOnSphere) rather than
// And straight through it is a curve too, now that the middle of the pad is
// the pole rather than a whole circle of the room. It was a genuine
// discontinuity -- the path arrived at one bearing and left at the opposite
// one -- and the pen was lifted at it. Nothing lifts it any more; this is the
// test that says so, and it fails the moment the mapping goes back to fanning
// the middle across a latitude.
TEST (DiscStepPieces, StraightThroughTheOriginIsACurveToo)
{
  for (float base : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
    {
      EXPECT_LT (worstDrawn (-0.06f, 0.f, 0.06f, 0.f, 0.5f, base), 0.1f)
          << "base " << base << ": the path through the pad's centre is torn";
      EXPECT_EQ (liftsAcross (-0.06f, 0.f, 0.06f, 0.f, 0.5f, base), 0)
          << "base " << base << ": the pen lifted where nothing is torn";
    }
}

/** Halving a piece has a floor, and at the disc's origin the swing does not.
 *  So the sampler runs out of depth with its two ends still far apart, and
 *  what it hands over is a chord across the sphere -- short enough to slip
 *  under the pen lift, long enough to see. That is the little straight hook at
 *  the end of each of a Clover's petals.
 *
 *  A piece it could not resolve is not a piece: the sampler has to say so, and
 *  the pen goes up rather than joining two points it knows nothing between.
 */
TEST (DiscStepPieces, APieceItCouldNotResolveIsNotDrawn)
{
  DiscSampling const how;

  for (float base : { 0.13f, 0.25f, 0.5f })
    EXPECT_LE (worstJoined (-0.06f, 0.f, 0.06f, 0.f, 0.5f, base), how.maxDrawn)
        << "base " << base
        << ": a line was joined across a piece the sampler gave up on";
}

/** The disc's origin has no bearing of its own, and a projection asked for one
 *  there answers with a fixed direction -- atan2(0, 0) is zero, which is one
 *  particular corner of the room. That used to send every shape with a vertex
 *  at the middle of the pad off to that corner and back.
 *
 *  It cannot any more: the middle of the pad is the pole, and at a pole every
 *  bearing is the same point, so there is nothing for atan2 to be wrong about.
 *  Kept as the test that says so.
 */
TEST (DiscStepPieces, TheOriginHasNoBearingLeftToGetWrong)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 0.5f;
  params.elevationBase = 0.13f;

  auto const at = [&] (float x, float y) {
    return heightMap.mapTo3D (Pos::fromCartesian (x, y, 0.f), params);
  };

  auto const middle = at (0.f, 0.f);

  // The pole, whatever bearing it is approached on -- so every way in lands on
  // the same point and none of them can disagree.
  EXPECT_NEAR (middle.z (), 1.f, 1e-4f);

  for (int i = 0; i < 8; ++i)
    {
      auto const bearing
          = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 8.f;
      auto const near = at (0.001f * std::cos (bearing),
                            0.001f * std::sin (bearing));

      EXPECT_NEAR (near.z (), 1.f, 1e-3f) << "bearing " << bearing;
    }
}
