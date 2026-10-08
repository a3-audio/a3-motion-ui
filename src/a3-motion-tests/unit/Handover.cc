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

#include <a3-motion-engine/flight/Handover.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <algorithm>
#include <cmath>

using namespace a3;

namespace
{

constexpr float unitSlack = 1e-5f;

Pos
direction (float azimuth, float elevation = 0.f)
{
  return Pos::fromSpherical (azimuth, elevation, 1.f);
}

/** The angle between two directions, in degrees. */
float
degreesBetween (Pos const &a, Pos const &b)
{
  auto const dot = a.x () * b.x () + a.y () * b.y () + a.z () * b.z ();
  auto const cosine = dot / (a.distance () * b.distance ());
  return std::acos (std::clamp (cosine, -1.f, 1.f)) * 180.f / pi<float> ();
}

void
expectNear (Pos const &actual, Pos const &expected, float slack = unitSlack)
{
  EXPECT_NEAR (actual.x (), expected.x (), slack);
  EXPECT_NEAR (actual.y (), expected.y (), slack);
  EXPECT_NEAR (actual.z (), expected.z (), slack);
}

}

TEST (Handover, WeightZeroIsFromAndOneIsTo)
{
  auto const from = direction (10.f, 20.f);
  auto const to = direction (-70.f, 5.f);
  expectNear (handover (from, to, 0.f), from);
  expectNear (handover (from, to, 1.f), to);
}

TEST (Handover, HalfwayIsBetweenOnTheShortWay)
{
  auto const halfway = handover (direction (10.f), direction (50.f), 0.5f);
  EXPECT_NEAR (halfway.azimuth (), 30.f, 0.5f);
  // the short way round from 170 to -170 crosses 180, not 0
  auto const acrossTheBack = handover (direction (170.f), direction (-170.f), 0.5f);
  EXPECT_NEAR (std::fabs (acrossTheBack.azimuth ()), 180.f, 0.5f);
}

TEST (Handover, OppositeDirectionsGoStraightToTheClip)
{
  auto const to = direction (-90.f);
  expectNear (handover (direction (90.f), to, 0.5f), to);
}

TEST (Handover, AnInvalidStartGoesToTheClip)
{
  auto const to = direction (40.f, 10.f);
  expectNear (handover (Pos::invalid, to, 0.3f), to);
  expectNear (handover (Pos{}, to, 0.3f), to); // no direction at all
}

TEST (Handover, SmoothstepIsFlatAtBothEnds)
{
  constexpr float h = 1e-4f;
  EXPECT_LT ((smoothstep (h) - smoothstep (0.f)) / h, 1e-3f);
  EXPECT_LT ((smoothstep (1.f) - smoothstep (1.f - h)) / h, 1e-3f);
  EXPECT_FLOAT_EQ (smoothstep (0.5f), 0.5f);
  EXPECT_FLOAT_EQ (smoothstep (-1.f), 0.f);
  EXPECT_FLOAT_EQ (smoothstep (2.f), 1.f);
}

TEST (Handover, TheResultIsAlwaysUnitLength)
{
  juce::Random dice (spreadSeed (8));
  for (auto i = 0; i < 10000; ++i)
    {
      auto const from = direction (360.f * dice.nextFloat () - 180.f,
                                   180.f * dice.nextFloat () - 90.f);
      auto const to = direction (360.f * dice.nextFloat () - 180.f,
                                 180.f * dice.nextFloat () - 90.f);
      auto const result = handover (from, to, dice.nextFloat ());
      ASSERT_TRUE (result.isValid ()) << "pair " << i;
      ASSERT_NEAR (result.distance (), 1.f, unitSlack) << "pair " << i;
    }
}

// Review Focus 2, the ORBIT -> CLIP half: over the one-beat glide no tick
// moves the sound further than the blend's steepest point allows, and the
// glide starts on the orbit and ends on the clip. A normalised lerp turns
// fastest halfway, at sin(span) / ((1 + cos(span)) / 2) per unit of weight,
// and the smoothstep is steepest there too (1.5).
TEST (Handover, TheGlideBackHasNoJump)
{
  auto const ticks = TempoClock::getTicksPerBeat ();
  auto const orbit = direction (-60.f, 15.f);
  auto const clip = direction (75.f, 30.f);
  auto const span = degreesBetween (orbit, clip) * pi<float> () / 180.f;
  auto const steepestBlend
      = std::sin (span) / (0.5f * (1.f + std::cos (span))) * 180.f / pi<float> ();
  constexpr float steepestSmoothstep = 1.5f;
  auto const widestStep
      = 1.1f * steepestSmoothstep * steepestBlend / static_cast<float> (ticks);

  auto previous = handover (orbit, clip, smoothstep (0.f));
  expectNear (previous, orbit);
  for (auto tick = 1; tick <= ticks; ++tick)
    {
      auto const progress = static_cast<float> (tick) / static_cast<float> (ticks);
      auto const now = handover (orbit, clip, smoothstep (progress));
      EXPECT_LE (degreesBetween (previous, now), widestStep) << "tick " << tick;
      previous = now;
    }
  expectNear (previous, clip);
}
