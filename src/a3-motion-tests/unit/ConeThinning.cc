/* A3 Motion UI
 * - a real-time DAW for spatial audio
 *
 * Copyright (C) 2024 Patric Schmitz
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <a3-motion-ui/components/SphereProjection.hh>

#include <gtest/gtest.h>

using namespace a3;

namespace
{

std::vector<juce::Point<float> >
alongX (int count, float spacing)
{
  std::vector<juce::Point<float> > points;
  for (auto i = 0; i < count; ++i)
    points.push_back ({ static_cast<float> (i) * spacing, 0.f });
  return points;
}

}

// The cone is ten nested strokes up to sixty-eight texels across. It cannot
// show the difference between two points two texels apart, but the stroker
// pays for both -- an arc at every joint, ten times over. Thinning by arc
// length is what buys that back, and these are the rules it may not break.

TEST (ConeThinning, KeepsEveryPointWhenTheyAreAlreadyFarApart)
{
  auto const points = alongX (5, 10.f);
  std::vector<bool> const lifts (points.size (), false);

  EXPECT_EQ (thinByArcLength (points, lifts, 4.f),
             (std::vector<std::size_t>{ 0, 1, 2, 3, 4 }));
}

TEST (ConeThinning, DropsPointsCloserThanTheSpacing)
{
  auto const points = alongX (9, 1.f);
  std::vector<bool> const lifts (points.size (), false);

  EXPECT_EQ (thinByArcLength (points, lifts, 4.f),
             (std::vector<std::size_t>{ 0, 4, 8 }));
}

// The pieces of the cone overlap by one point: each begins where the last one
// ended. A thinning that dropped the last point would pull the pieces apart.
TEST (ConeThinning, AlwaysKeepsTheLastPointHoweverCloseItIs)
{
  auto points = alongX (5, 1.f);
  points.push_back ({ 4.1f, 0.f });
  std::vector<bool> const lifts (points.size (), false);

  auto const kept = thinByArcLength (points, lifts, 4.f);
  ASSERT_FALSE (kept.empty ());
  EXPECT_EQ (kept.back (), points.size () - 1);
}

// A lift is where the pen goes up -- at a take's gaps and at the disc's
// origin. Dropping one would rule a line straight across the sphere through
// somewhere the sound never was, which is the very thing addPoint() lifts the
// pen to avoid.
TEST (ConeThinning, NeverDropsAPenLiftNorThePointThatEndsTheRunBeforeIt)
{
  auto const points = alongX (9, 1.f);
  std::vector<bool> lifts (points.size (), false);
  lifts[5] = true;

  auto const kept = thinByArcLength (points, lifts, 4.f);

  EXPECT_NE (std::find (kept.begin (), kept.end (), std::size_t{ 5 }),
             kept.end ());
  EXPECT_NE (std::find (kept.begin (), kept.end (), std::size_t{ 4 }),
             kept.end ());
}

// Measured from the last point actually kept, not from the point before it in
// the list: measured per point, a run of steps just under the spacing would
// keep every one of them and thin nothing at all.
TEST (ConeThinning, MeasuresFromTheLastKeptPoint)
{
  std::vector<juce::Point<float> > points;
  for (auto i = 0; i < 10; ++i)
    points.push_back ({ static_cast<float> (i) * 3.9f, 0.f });
  std::vector<bool> const lifts (points.size (), false);

  auto const kept = thinByArcLength (points, lifts, 4.f);
  EXPECT_LT (kept.size (), points.size () - 1);
}

TEST (ConeThinning, ASpacingOfZeroThinsNothing)
{
  auto const points = alongX (4, 1.f);
  std::vector<bool> const lifts (points.size (), false);

  EXPECT_EQ (thinByArcLength (points, lifts, 0.f),
             (std::vector<std::size_t>{ 0, 1, 2, 3 }));
}
