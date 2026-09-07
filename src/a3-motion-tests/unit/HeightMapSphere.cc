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

// ── The pad is wrapped around the base ───────────────────────────────────

/** Where the base is, the middle of the pad is -- and the pad grows out of it
 *  in every direction, which is what makes it a cap rather than a cone with a
 *  side it has to pick.
 *
 *  The rule this replaces grew the figure towards whichever pole was further
 *  away, and had to be told which way that was once a sweep started moving
 *  the base through the middle. There is nothing to tell any more. */
TEST (HeightMapSphere, TheMiddleOfThePadLandsOnTheBase)
{
  HeightMapSphere heightMap;

  for (float base : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
    {
      ElevationParams params;
      params.reach = 0.5f;
      params.elevationBase = base;

      auto const centre
          = heightMap.mapTo3D (Pos::fromCartesian (0.f, 0.f, 0.f), params);

      // Colatitude counted from the north pole, as a fraction, is the base.
      auto const frac
          = std::atan2 (std::sqrt (centre.x () * centre.x ()
                                   + centre.y () * centre.y ()),
                        centre.z ())
            / juce::MathConstants<float>::pi;

      EXPECT_NEAR (frac, base, 1e-4f) << "base " << base;
    }
}

/** And the pad reaches out of it evenly: two points the same distance from
 *  the pad's centre are the same distance from the base, whichever bearing
 *  they are on. A shear could not say that -- it stretched one way and
 *  squashed the other. */
TEST (HeightMapSphere, ThePadReachesOutOfTheBaseEvenly)
{
  HeightMapSphere heightMap;

  ElevationParams params;
  params.reach = 0.5f;
  params.elevationBase = 0.35f;

  auto const centre
      = heightMap.mapTo3D (Pos::fromCartesian (0.f, 0.f, 0.f), params);
  auto const angleFromCentre = [&] (Pos const &at) {
    auto const dot = at.x () * centre.x () + at.y () * centre.y ()
                     + at.z () * centre.z ();
    return std::acos (std::clamp (dot, -1.f, 1.f));
  };

  auto const r = 0.6f;
  auto const first = angleFromCentre (
      heightMap.mapTo3D (Pos::fromCartesian (r, 0.f, 0.f), params));

  for (int i = 1; i < 8; ++i)
    {
      auto const bearing
          = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 8.f;
      auto const at = heightMap.mapTo3D (
          Pos::fromCartesian (r * std::cos (bearing), r * std::sin (bearing),
                              0.f),
          params);

      EXPECT_NEAR (angleFromCentre (at), first, 1e-4f)
          << "bearing " << bearing;
    }
}

// ── The disc is wrapped around the base, not sheared towards it ──────────

/** A movement that is smooth on the pad has to be smooth in the room.
 *
 *  It was not, once the base left the pole. The map took the disc's angle as
 *  the *global* azimuth and its radius as a change in colatitude, which is a
 *  proper wrapping of the pad only when the base is a pole: anywhere else the
 *  disc's origin stands for "this colatitude, any azimuth" -- a whole circle
 *  of directions -- and two neighbouring ticks either side of the pad's centre
 *  land on opposite sides of it.
 *
 *  Measured on a Clover's 2048 ticks with a reach of a half: at a base of 0
 *  the largest step between two ticks was the average one, at 0.25 it was 117
 *  times it, at 0.5 it was 164 times. That is the sound teleporting, four
 *  times a lap, not a line drawn badly.
 */
TEST (HeightMapSphere, AFigureThroughTheDiscsCentreStaysInOnePiece)
{
  HeightMapSphere heightMap;

  // A path straight across the disc and through its origin, sampled evenly --
  // the way a take's ticks cross the middle of the pad.
  auto const worstAgainstAverage = [&] (float base) {
    ElevationParams params;
    params.reach = 0.5f;
    params.elevationBase = base;

    auto worst = 0.f;
    auto total = 0.f;
    auto counted = 0;
    Pos previous;

    for (int i = 0; i <= 400; ++i)
      {
        auto const x = -0.8f + 1.6f * static_cast<float> (i) / 400.f;
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

  for (float base : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
    EXPECT_LT (worstAgainstAverage (base), 2.f)
        << "base " << base
        << ": one step across the pad's centre is many times every other, "
           "which is the sound jumping";
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
