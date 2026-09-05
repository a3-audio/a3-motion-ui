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

#include <a3-motion-engine/TrajectoryBridges.hh>

#include <algorithm>
#include <cmath>

using namespace a3;

namespace
{
/** A short crawl along the left edge and then one jump clean across.
 *
 *  Ten small steps rather than two: the jump threshold is eight times the
 *  MEDIAN step, so a fixture where half the steps are jumps has no jumps at
 *  all -- which is true of a real take too, and worth knowing before writing
 *  a fixture that looks nothing like one. */
constexpr size_t crawlLength = 10;
constexpr size_t gapAt = crawlLength - 1;

std::vector<Pos>
aRunWithOneWideGap ()
{
  std::vector<Pos> ticks;
  for (size_t i = 0; i < crawlLength; ++i)
    ticks.push_back (
        Pos::fromCartesian (-0.9f + 0.02f * static_cast<float> (i), 0.f, 0.f));

  // 1.62 from where the crawl ended.
  ticks.push_back (Pos::fromCartesian (0.9f, 0.f, 0.f));
  return ticks;
}
}

// The rule the whole feature is: a gap shorter than the reach is drawn
// through, a longer one stays a jump.
TEST (TrajectoryBridges, ReachDecidesWhichGapsAreClosed)
{
  auto const ticks = aRunWithOneWideGap ();

  // The gap is 1.62 across; half the diameter is 1.0, so it does not fit.
  auto const tooShort = planBridges (ticks, 0.5f, 0, 1);
  EXPECT_FALSE (tooShort.bridged (gapAt))
      << "a 1.62 gap fitted into a 1.0 reach";

  // Nine tenths of the diameter is 1.8, and it does.
  auto const longEnough = planBridges (ticks, 0.9f, 0, 1);
  EXPECT_TRUE (longEnough.bridged (gapAt));
}

// The two ends of the dial, which is what the pot's travel has to mean.
TEST (TrajectoryBridges, NothingAtZeroAndEverythingAtOne)
{
  auto const ticks = aRunWithOneWideGap ();

  EXPECT_TRUE (planBridges (ticks, 0.f, 0, 1).bridges.empty ());
  EXPECT_TRUE (planBridges (ticks, 1.f, 0, 1).bridged (gapAt))
      << "full reach left a gap open";
}

// The base rule, and the one a performer can predict.
TEST (TrajectoryBridges, WithoutBiasABridgeGoesToTheNextTickInTime)
{
  auto const plan = planBridges (aRunWithOneWideGap (), 1.f, 0, 1);
  ASSERT_TRUE (plan.via (gapAt).has_value ());
  EXPECT_EQ (*plan.via (gapAt), gapAt + 1);
}
