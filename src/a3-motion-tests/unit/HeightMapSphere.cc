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


// The middle of a figure used to land on the base, and that is what tore it:
// one point of the pad standing for a whole latitude of the room. It lands on
// a pole now -- see AFigureThroughTheDiscsCentreDoesNotJump and
// TheMiddleOfThePadIsThePole, which are what that was replaced by.
//
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

// The cone grows one way and one way only: down, from the base towards the
// floor. It used to grow towards whichever pole was further away, which is a
// rule with a jump in it at a base of exactly one half -- right where sway
// spends most of its time.
TEST (HeightMapSphere, ItAlwaysGrowsDownwards)
{
  HeightMapSphere heightMap;

  // Outside the run from the pad's middle, which goes the other way by
  // definition -- towards a pole rather than away from the base. See
  // TheLongerTheRunTheMoreOfThePadItIsGiven for how much pad that takes.
  auto const near = Pos::fromCartesian (0.75f, 0.f, 0.f);

  for (float base : { 0.f, 0.2f, 0.49f, 0.5f, 0.51f, 0.8f })
    EXPECT_GT (fracOf (heightMap.mapTo3D (near, baseParams (base))), base)
        << "base " << base;
}

// And what runs out of sphere at the bottom comes up the other side rather
// than piling onto the pole -- the figure wraps over the wall.
TEST (HeightMapSphere, PastTheFloorItWrapsBackUp)
{
  HeightMapSphere heightMap;
  auto const edge = Pos::fromCartesian (1.41f, 0.f, 0.f);

  // At this base and reach the edge of the pad is a quarter turn past the
  // south pole, so it comes back up to the same colatitude on the far side.
  auto const params = baseParams (0.8f);
  auto const theta = fracOf (heightMap.mapTo3D (edge, params));

  EXPECT_NEAR (theta, 2.f - 0.8f - 0.4756f, 1e-2f);
  EXPECT_LT (theta, 1.f) << "it must stay on the sphere, not run off it";
}

// Stepping the base across the equator moves the figure by as much as the
// step itself. The old "towards the further pole" rule reversed the whole
// cone there: a base of 0.499 and one of 0.501 put a point a third of the
// sphere apart, which is a sweep through the middle audibly jumping.
TEST (HeightMapSphere, NothingHappensAtTheEquator)
{
  HeightMapSphere heightMap;
  auto const at = Pos::fromCartesian (0.7f, 0.f, 0.f);

  auto const below = fracOf (heightMap.mapTo3D (at, baseParams (0.499f)));
  auto const above = fracOf (heightMap.mapTo3D (at, baseParams (0.501f)));

  EXPECT_NEAR (above - below, 0.002f, 1e-3f);
}

// mapTo2D is the exact inverse, and has to stay so with a base in play: a
// recording is written through it and played back through mapTo3D, so a
// mismatch would move every take the moment its base was touched.
//
// It is exact everywhere the figure is still on its way down, and at both
// poles. The one place it cannot be is where the figure has gone over the
// floor and come back up: a direction there is reached twice, once on the way
// down and once on the way back, and the point alone does not say which. The
// inverse picks the way down. The values below stay on the single-valued
// side, which is where a recording is actually made.
TEST (HeightMapSphere, TheInverseStillComesBackWithABase)
{
  HeightMapSphere heightMap;

  // Out on the band, which is where it is exact -- see the note in mapTo2D
  // about the run from the pad's middle covering the same latitudes twice.
  for (float base : { 0.f, 0.3f, 0.5f, 1.f })
    for (float x : { 0.5f, 0.8f })
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


/** The pad's radius is height and nothing else: two points the same distance
 *  from the pad's centre come out at the same height, whichever bearing they
 *  are on, and the figure stays centred on the room's vertical axis however
 *  the base is moved.
 *
 *  That is what an elevation control has to mean. The alternative -- wrapping
 *  the pad around the base as a cap -- is continuous everywhere but carries
 *  the figure's own centre sideways across the room as the base moves, which
 *  reads on the sphere as the whole view tilting. */
TEST (HeightMapSphere, ThePadsRadiusIsHeightWhateverTheBearing)
{
  HeightMapSphere heightMap;

  auto const params = baseParams (0.35f);
  auto const r = 0.6f;

  auto const first
      = fracOf (heightMap.mapTo3D (Pos::fromCartesian (r, 0.f, 0.f), params));

  for (int i = 1; i < 8; ++i)
    {
      auto const bearing
          = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 8.f;
      auto const at = heightMap.mapTo3D (
          Pos::fromCartesian (r * std::cos (bearing), r * std::sin (bearing),
                              0.f),
          params);

      EXPECT_NEAR (fracOf (at), first, 1e-4f) << "bearing " << bearing;

      // ... and it is the bearing it was given, so the figure keeps its shape
      // in plan rather than being turned by its own height.
      auto const drawn = std::atan2 (at.y (), at.x ());
      auto const apart = std::abs (
          std::remainder (drawn - bearing, juce::MathConstants<float>::twoPi));
      EXPECT_NEAR (apart, 0.f, 1e-4f) << "bearing " << bearing;
    }
}

/** And it no longer jumps.
 *
 *  One point of the pad standing for a whole circle of the room is what tore a
 *  figure crossing the middle: two neighbouring ticks either side of it landed
 *  on opposite sides of a latitude. Measured across the pad's centre with a
 *  reach of a half, the worst step between neighbouring ticks was the average
 *  one at a base of nought, a hundred and seventeen times it at a quarter and
 *  a hundred and sixty-four times at the equator.
 *
 *  A circle can only be closed up continuously by filling what it bounds, and
 *  the one thing a latitude bounds is a cap -- so the innermost tenth of the
 *  pad is that cap. Crossing the middle is a run to the ceiling and back
 *  rather than a jump across the room, and every point of it is a place the
 *  sound actually goes.
 */
TEST (HeightMapSphere, AFigureThroughTheDiscsCentreDoesNotJump)
{
  HeightMapSphere heightMap;

  auto const worstAgainstAverage = [&] (float base) {
    auto const params = baseParams (base);

    auto worst = 0.f;
    auto total = 0.f;
    auto counted = 0;
    Pos previous;

    for (int i = 0; i <= 2000; ++i)
      {
        auto const x = -0.8f + 1.6f * static_cast<float> (i) / 2000.f;
        auto const at
            = heightMap.mapTo3D (Pos::fromCartesian (x, 0.f, 0.f), params);

        if (i > 0)
          {
            auto const step
                = std::sqrt (std::pow (at.x () - previous.x (), 2.f)
                             + std::pow (at.y () - previous.y (), 2.f)
                             + std::pow (at.z () - previous.z (), 2.f));
            worst = std::max (worst, step);
            total += step;
            ++counted;
          }

        previous = at;
      }

    return worst / std::max (1e-6f, total / static_cast<float> (counted));
  };

  // Under six, where it was over a hundred. The run is quicker than the rest
  // of the figure and that is the point of it -- a tenth of the pad's width
  // covers a quarter of the way to the pole, so the sound swoops there and
  // back. Quick is a movement; a hundred times is not.
  for (float base : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
    EXPECT_LT (worstAgainstAverage (base), 6.f)
        << "base " << base
        << ": a step across the pad's centre is many times every other, "
           "which is the sound jumping";
}

/** The middle of the pad is *a* pole, whatever the base. That is the whole of
 *  why it no longer tears: a point maps to a point. Which of the two it is is
 *  TheRunFromTheMiddleTakesTheNearerPole's business. */
TEST (HeightMapSphere, TheMiddleOfThePadIsThePole)
{
  HeightMapSphere heightMap;

  for (float base : { 0.f, 0.25f, 0.5f, 0.9f })
    for (float reach : { 0.5f, -0.5f })
      {
        auto const at = heightMap.mapTo3D (Pos::fromCartesian (0.f, 0.f, 0.f),
                                           baseParams (base, reach));
        auto const frac = fracOf (at);

        EXPECT_TRUE (std::abs (frac) < epsilon
                     || std::abs (frac - 1.f) < epsilon)
            << "base " << base << " reach " << reach << ": frac " << frac;
      }
}

/** And it runs to the *nearer* pole.
 *
 *  The pad's middle has to be a pole -- that is the topology, see
 *  AFigureThroughTheDiscsCentreDoesNotJump -- but there are two of them, and
 *  which one is picked is the difference between a fold and a catapult. It
 *  used to be the one on the far side of the figure, on the reasoning that the
 *  run should go into room the figure was not already using. The price was
 *  paid at exactly the settings elevation is most often left at: with the base
 *  near the floor the innermost tenth of the pad spanned nine tenths of the
 *  sphere, so every pass near the middle of a figure was flung to the ceiling
 *  and back, and a pass that missed the middle by a hair stopped dead in open
 *  room -- the hole in the line that this test is here to keep shut.
 *
 *  The nearer pole is never the longer run and is usually far shorter, and it
 *  is room the figure is standing next to anyway.
 */
TEST (HeightMapSphere, TheRunFromTheMiddleTakesTheNearerPole)
{
  HeightMapSphere heightMap;

  // The fold's outer rim: where the middle's run has to start from, a tenth of
  // the way out.
  auto const rimFrac = [&] (ElevationParams const &params) {
    return fracOf (heightMap.mapTo3D (
        Pos::fromCartesian (0.1f * 1.41421356f, 0.f, 0.f), params));
  };

  for (auto const &params :
       { baseParams (0.892f, 0.108f), baseParams (0.75f, 0.25f),
         baseParams (0.6f, 0.4f), baseParams (0.28f, 0.65f),
         baseParams (0.1f, 0.5f), baseParams (0.4f, -0.35f) })
    {
      auto const rim = rimFrac (params);
      auto const middle = fracOf (
          heightMap.mapTo3D (Pos::fromCartesian (0.f, 0.f, 0.f), params));

      auto const nearer = rim <= 0.5f ? 0.f : 1.f;
      EXPECT_NEAR (middle, nearer, epsilon)
          << "base " << params.elevationBase << " reach " << params.reach
          << ": the run goes to the pole further from the figure";

      EXPECT_LE (std::abs (rim - middle), 0.5f + epsilon)
          << "base " << params.elevationBase << " reach " << params.reach
          << ": the middle of the pad is more than half the sphere from the "
             "rest of the figure";
    }
}

/** The longer the run, the more of the pad it is given.
 *
 *  The run from the pad's middle to a pole is as long as the base is far from
 *  that pole -- half the sphere with the base at ear height -- and it used to
 *  be crammed into a tenth of the pad however long it was. Drawn, that is a
 *  straight needle across the room; and a pass that missed the middle by a
 *  hair turned back halfway along it, ending in open air.
 *
 *  So the tenth is a floor, not the figure. A long run is spread over up to a
 *  third of the pad, which is the same gradient the figure itself has to
 *  within a factor of three rather than ten, and a near miss then turns back
 *  close enough to the pole that it reads as the middle of a figure rather
 *  than as a broken line.
 *
 *  The price, and it is the reason this is capped: a third of the pad's radius
 *  is a third of every figure's middle, funnelled towards a pole instead of
 *  standing on the base's latitude. Most visible with little reach, where
 *  there is not much figure for it to be a third of.
 */
TEST (HeightMapSphere, TheLongerTheRunTheMoreOfThePadItIsGiven)
{
  HeightMapSphere heightMap;

  auto const fracAt = [&] (float rNorm, ElevationParams const &params) {
    return fracOf (heightMap.mapTo3D (
        Pos::fromCartesian (rNorm * 1.41421356f, 0.f, 0.f), params));
  };

  // Ear height: the longest run there is, and a fifth of the pad is still
  // inside it -- on the way to the pole rather than standing near the base.
  EXPECT_GT (fracAt (0.2f, baseParams (0.5f)), 0.7f)
      << "the run is still crammed into a tenth of the pad";

  // Near the ceiling there is barely any run, and the pad is the band it
  // always was by a fifth out.
  EXPECT_LT (fracAt (0.2f, baseParams (0.05f)), 0.15f)
      << "a short run took more of the pad than it needs";
}

/** The run is over by a tenth of the pad. Outside it the map is the band it
 *  always was, so elevation still moves a figure's height and not the figure,
 *  which is the reason the band was chosen over a cap in the first place. */
TEST (HeightMapSphere, TheRunToThePoleIsOverByATenthOfThePad)
{
  HeightMapSphere heightMap;

  auto const params = baseParams (0.35f);

  for (float r : { 0.25f, 0.6f, 1.f, 1.41f })
    for (int i = 0; i < 8; ++i)
      {
        auto const bearing
            = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 8.f;
        auto const at = heightMap.mapTo3D (
            Pos::fromCartesian (r * std::cos (bearing), r * std::sin (bearing),
                                0.f),
            params);

        auto const drawn = std::atan2 (at.y (), at.x ());
        EXPECT_NEAR (std::abs (std::remainder (
                         drawn - bearing, juce::MathConstants<float>::twoPi)),
                     0.f, 1e-4f)
            << "r " << r << " bearing " << bearing;
      }
}

/** A figure that runs into the ceiling travels *along* it.
 *
 *  clipTop and clipBottom bound where the sound may go, and a point pushed
 *  past one of them keeps its bearing and gives up only its height. So a run
 *  that would have gone over the top comes out as a run around the ceiling,
 *  still moving, rather than as a pile of points on one spot.
 */
TEST (HeightMapSphere, WhatIsCutOffAtTheCeilingRunsAlongIt)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 1.f;          // reaches well past the cut
  params.elevationBase = 0.f;  // centred overhead
  params.clipTop = 0.3f;       // and the top three tenths are cut away

  // A ring of the pad well inside the cut region: every one of these would
  // have been above the ceiling.
  std::vector<Pos> ring;
  for (int i = 0; i < 16; ++i)
    {
      auto const bearing
          = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 16.f;
      ring.push_back (heightMap.mapTo3D (
          Pos::fromCartesian (0.1f * std::cos (bearing),
                              0.1f * std::sin (bearing), 0.f),
          params));
    }

  // All of them sit on the ceiling itself...
  auto const ceiling = std::cos (params.clipTop
                                 * juce::MathConstants<float>::pi);
  for (auto const &at : ring)
    EXPECT_NEAR (at.z (), ceiling, 1e-4f);

  // ... and they are spread around it rather than heaped on one point. Each
  // keeps the bearing it arrived on, so the ring stays a ring.
  for (size_t i = 0; i < ring.size (); ++i)
    {
      auto const &a = ring[i];
      auto const &b = ring[(i + 1) % ring.size ()];
      auto const apart = std::sqrt (std::pow (a.x () - b.x (), 2.f)
                                    + std::pow (a.y () - b.y (), 2.f));

      EXPECT_GT (apart, 0.01f)
          << "point " << i << " landed on top of its neighbour: the figure "
          << "was flattened onto the ceiling instead of running along it";
    }
}

// ── Reach runs both ways ────────────────────────────────────────────────

/** The cone grows down from the base, and with a negative reach it grows up.
 *
 *  It grew one way only once the "towards the further pole" rule went, which
 *  left a figure sitting at the ceiling with nowhere to be put: base at the
 *  top and reach is the only thing that decides where the rest of it goes.
 *  The sign is the direction and the size is the extent, which is how spin,
 *  swell and the squeezes already read.
 */
TEST (HeightMapSphere, ANegativeReachGrowsUpwards)
{
  HeightMapSphere heightMap;
  auto const at = Pos::fromCartesian (0.7f, 0.f, 0.f);

  auto const base = 0.6f;
  auto const down = fracOf (heightMap.mapTo3D (at, baseParams (base, 0.5f)));
  auto const up = fracOf (heightMap.mapTo3D (at, baseParams (base, -0.5f)));

  EXPECT_GT (down, base) << "a positive reach must still grow down";
  EXPECT_LT (up, base) << "a negative reach must grow up";

  // Mirror images of one another about the base: the same figure, upside
  // down, not a different one.
  EXPECT_NEAR (down - base, base - up, epsilon);
}

// And what runs off the ceiling wraps over it, the way the floor already
// does: a figure pushed past the top comes down the other side rather than
// piling onto the pole.
TEST (HeightMapSphere, PastTheCeilingItWrapsBackDown)
{
  HeightMapSphere heightMap;
  auto const edge = Pos::fromCartesian (1.41f, 0.f, 0.f);

  auto const frac = fracOf (heightMap.mapTo3D (edge, baseParams (0.2f, -0.5f)));

  EXPECT_GT (frac, 0.f) << "it must stay on the sphere, not run off it";
  EXPECT_NEAR (frac, 0.4756f - 0.2f, 1e-2f);
}

// The inverse follows the sign too -- a take is written through it and played
// back through mapTo3D, so a mismatch moves every recording the moment reach
// is turned past nothing.
TEST (HeightMapSphere, TheInverseFollowsTheSignOfReach)
{
  HeightMapSphere heightMap;

  for (float reach : { 0.5f, -0.5f })
    for (float x : { 0.5f, 0.8f })
      {
        auto const params = baseParams (0.5f, reach);
        auto const there = Pos::fromCartesian (x, 0.2f, 0.f);

        auto const round
            = heightMap.mapTo2D (heightMap.mapTo3D (there, params), params);

        EXPECT_NEAR (round.x (), there.x (), 1e-3f) << "reach " << reach;
        EXPECT_NEAR (round.y (), there.y (), 1e-3f) << "reach " << reach;
      }
}

/** Past a pole the map folds -- a direction there is reached twice, once on
 *  the way out and once on the way back -- so the inverse cannot be exact
 *  there and picks the way out. What it must not do is pick the wrong branch
 *  on the *near* side, which is the whole of where recording happens: with a
 *  negative reach that side is above the base, not below it. */
TEST (HeightMapSphere, TheInverseTakesTheNearSideOfTheBase)
{
  HeightMapSphere heightMap;

  for (float reach : { 0.6f, -0.6f })
    for (float x : { 0.6f, 0.9f })
      {
        auto const params = baseParams (0.5f, reach);
        auto const there = Pos::fromCartesian (x, 0.1f, 0.f);

        auto const round
            = heightMap.mapTo2D (heightMap.mapTo3D (there, params), params);

        EXPECT_NEAR (round.x (), there.x (), 1e-3f)
            << "reach " << reach << " at " << x;
        EXPECT_NEAR (round.y (), there.y (), 1e-3f)
            << "reach " << reach << " at " << x;
      }
}
