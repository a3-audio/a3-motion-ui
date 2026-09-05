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

namespace
{
/** Three separate runs with wide gaps between them, so a gap has more than one
 *  place it could lead. The third run sits close to where the first one ends,
 *  which is what makes "the nearest one" a different answer from "the next one
 *  in time". */
constexpr size_t runLength = 10;
constexpr size_t endOfFirstRun = runLength - 1;
constexpr size_t startOfSecondRun = runLength;
constexpr size_t startOfThirdRun = 2 * runLength;

std::vector<Pos>
threeRunsApart ()
{
  std::vector<Pos> ticks;
  auto run = [&ticks] (float x, float y) {
    for (size_t i = 0; i < runLength; ++i)
      ticks.push_back (Pos::fromCartesian (
          x + 0.02f * static_cast<float> (i), y, 0.f));
  };

  run (-0.6f, 0.0f);   // ends at -0.42, 0
  run (0.5f, 0.3f);    // far away
  run (-0.3f, 0.15f);  // close to where the first one ended
  return ticks;
}
}

// Turned left, a bridge takes the smoothest way out it can find: the nearest
// run, rather than the one that happens to come next in time.
TEST (TrajectoryBridges, NegativeBiasGoesToTheNearestReachableRun)
{
  auto const ticks = threeRunsApart ();
  auto const plan = planBridges (ticks, 1.f, -4, seedForTicks (ticks));

  ASSERT_TRUE (plan.via (endOfFirstRun).has_value ());
  EXPECT_EQ (*plan.via (endOfFirstRun), startOfThirdRun)
      << "the far run was chosen over the near one";
}

// Turned to the middle, the base rule holds however far the reach goes.
TEST (TrajectoryBridges, WithoutBiasItIsStillTheNextRunInTime)
{
  auto const ticks = threeRunsApart ();
  auto const plan = planBridges (ticks, 1.f, 0, seedForTicks (ticks));

  ASSERT_TRUE (plan.via (endOfFirstRun).has_value ());
  EXPECT_EQ (*plan.via (endOfFirstRun), startOfSecondRun);
}

// What cannot be reproduced cannot be saved: the same clip at the same setting
// has to go the same way, restart or no restart.
TEST (TrajectoryBridges, TheSameSeedGivesTheSamePlan)
{
  auto const ticks = threeRunsApart ();
  auto const seed = seedForTicks (ticks);

  auto const first = planBridges (ticks, 1.f, 4, seed);
  auto const second = planBridges (ticks, 1.f, 4, seed);

  ASSERT_EQ (first.bridges.size (), second.bridges.size ());
  for (size_t i = 0; i < first.bridges.size (); ++i)
    {
      EXPECT_EQ (first.bridges[i].fromTick, second.bridges[i].fromTick);
      EXPECT_EQ (first.bridges[i].viaTick, second.bridges[i].viaTick);
    }
}

// The seed follows the movement, not the name -- renaming a clip must not
// change where it goes.
TEST (TrajectoryBridges, TheSeedFollowsTheTicks)
{
  auto const ticks = threeRunsApart ();
  EXPECT_EQ (seedForTicks (ticks), seedForTicks (ticks));

  auto moved = ticks;
  moved[3] = Pos::fromCartesian (0.42f, -0.3f, 0.f);
  EXPECT_NE (seedForTicks (ticks), seedForTicks (moved));
}

// Turning the pot adds departures rather than redealing them: what strays at
// +1 still strays at +4. A pot that reshuffled on every degree could not be
// dialled in -- you would never get back the one you liked.
TEST (TrajectoryBridges, TheMagnitudeMixesMonotonically)
{
  auto const ticks = threeRunsApart ();
  auto const seed = seedForTicks (ticks);

  auto straying = [&] (int bias) {
    std::vector<index_t> out;
    for (auto const &b : planBridges (ticks, 1.f, bias, seed).bridges)
      if (b.viaTick != (b.fromTick + 1) % ticks.size ())
        out.push_back (b.fromTick);
    return out;
  };

  auto const gentle = straying (1);
  auto const full = straying (4);

  EXPECT_LE (gentle.size (), full.size ());
  for (auto const tick : gentle)
    EXPECT_NE (std::find (full.begin (), full.end (), tick), full.end ())
        << "tick " << tick << " strayed at +1 but not at +4";
}

// With nothing else in range there is nothing to choose, so the base rule
// holds whatever the bias says.
TEST (TrajectoryBridges, WithNoAlternativeInRangeTheBaseRuleHolds)
{
  auto const ticks = aRunWithOneWideGap ();
  auto const plan = planBridges (ticks, 0.9f, 4, seedForTicks (ticks));

  ASSERT_TRUE (plan.via (gapAt).has_value ());
  EXPECT_EQ (*plan.via (gapAt), gapAt + 1);
}
