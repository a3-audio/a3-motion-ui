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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TrajectoryBridges.hh>

#include <cmath>
#include <vector>

using namespace a3;

namespace
{
/** A take recorded in one continuous pass: 300 degrees of a circle, so the
 *  seam between its last tick and its first is a real gap of the size a hand
 *  leaves. One run, and the seam's two ends are both in it -- which is what
 *  every continuously recorded take looks like, and the case the window's
 *  "half of an end" rule does not cover. */
std::vector<Pos>
anOpenCircle (int n)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < n; ++i)
    {
      auto const a = (300.0 * M_PI / 180.0) * i / (n - 1);
      ticks.push_back (Pos::fromCartesian (0.6f * std::cos (a),
                                           0.6f * std::sin (a), 0.f));
    }
  return ticks;
}
}

// A crossing borrows time from the ends it joins. It must not borrow the whole
// take: what is inside the window is two straight lines, so a window that
// covers everything replaces the recorded figure with a triangle and the blob
// arrives at the landing point travelling almost exactly backwards.
//
// Measured before this rule existed, at a fade reach of 1 on a 48-tick take:
// leave 24, via 0, window 47, rejoin 23 -- forty-seven of forty-eight ticks
// inside one crossing, and one tick of the take still played.
TEST (CrossingWindow, ACrossingNeverSwallowsMoreThanHalfTheTake)
{
  auto const ticks = anOpenCircle (48);

  for (auto const reach : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
    {
      auto const plan = planBridges (ticks, reach, 0, 1234);

      for (auto const &bridge : plan.bridges)
        EXPECT_LE (bridge.windowTicks, ticks.size () / 2)
            << "reach " << reach << ", window " << bridge.windowTicks;
    }
}

// The rule the failure is really about: a crossing leaves from a run and comes
// back to it, and there has to be some of that run left to come back to. With
// both ends taking half of the same run, the two halves met and the run was
// gone.
TEST (CrossingWindow, TheRunACrossingLeavesFromIsStillThereToReturnTo)
{
  auto const ticks = anOpenCircle (48);
  auto const plan = planBridges (ticks, 1.f, 0, 1234);

  ASSERT_FALSE (plan.bridges.empty ());

  for (auto const &bridge : plan.bridges)
    {
      auto const covered = static_cast<size_t> (bridge.windowTicks);
      EXPECT_LT (covered, ticks.size ())
          << "the crossing covers the whole take";
      EXPECT_GE (ticks.size () - covered, ticks.size () / 2)
          << "less than half the take is still played as recorded";
    }
}

// A take made of separate taps is the case the window was designed for, and
// it must keep working: there the two ends are different runs and each may
// still give up half of itself.
TEST (CrossingWindow, SeparateRunsStillGiveUpHalfOfEachEnd)
{
  std::vector<Pos> ticks;
  auto run = [&ticks] (float x, float y) {
    for (int i = 0; i < 10; ++i)
      ticks.push_back (
          Pos::fromCartesian (x + 0.02f * static_cast<float> (i), y, 0.f));
  };
  run (-0.7f, 0.0f);
  run (0.4f, 0.35f);
  run (-0.2f, -0.4f);

  auto const plan = planBridges (ticks, 1.f, 0, 1234);

  ASSERT_FALSE (plan.bridges.empty ());
  // Something is still reserved -- the fix must not shrink these to nothing.
  auto reserved = false;
  for (auto const &bridge : plan.bridges)
    if (bridge.windowTicks > 1)
      reserved = true;
  EXPECT_TRUE (reserved);
}

namespace
{
std::shared_ptr<Pattern>
aPatternOf (std::vector<Pos> const &ticks)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->resize (ticks.size ());
  for (index_t tick = 0; tick < ticks.size (); ++tick)
    pattern->setTick (tick, ticks[tick]);
  pattern->markComplete ();
  return pattern;
}

/** How sharply the blob turns against itself, worst case, over a stretch of
 *  the take. The cosine of the angle between two consecutive steps: 1 is
 *  straight on, 0 is a right-angle corner, -1 is straight back.
 *
 *  This is the symptom measured the way it was reported -- "the blob briefly
 *  moves the other way" -- and it is shape-independent, which three earlier
 *  attempts at measuring it were not. Watching x, or watching the polar
 *  angle, both call correct behaviour a failure: a take that runs out and
 *  loops *must* come back, and a crossing walked as a straight chord *must*
 *  sweep its angle backwards. Only the blob turning against its own direction
 *  of travel is the fault.
 *
 *  Steps of no length are skipped: standing still is not a reversal, and
 *  standing still is what a tapped take does between taps. */
float
worstTurnAcross (Pattern &pattern, double from, double to, double step)
{
  auto worst = 1.f;
  auto previous = pattern.getInterpolatedTick (from);
  float px = 0.f, py = 0.f;
  auto havePrevious = false;

  for (auto t = from + step; t <= to; t += step)
    {
      auto const at = pattern.getInterpolatedTick (t);
      auto const dx = at.x () - previous.x ();
      auto const dy = at.y () - previous.y ();
      auto const length = std::sqrt (dx * dx + dy * dy);

      if (length > 1e-6f)
        {
          if (havePrevious)
            worst = std::min (worst, (px * dx + py * dy)
                                         / (std::sqrt (px * px + py * py)
                                            * length));
          px = dx;
          py = dy;
          havePrevious = true;
        }
      previous = at;
    }

  return worst;
}
}

// The bug as it was seen: crossing the loop point, the blob made a short
// movement in the wrong direction. At a fade reach of 1 it was not short --
// the crossing had swallowed the take, and the turn at the landing point
// measured cos -0.997, which is 176 degrees, the blob running back down the
// line it had just come up.
//
// A corner at the landing point is by design -- the crossing goes out to a
// point and then on to where it rejoins -- so this does not ask for none. It
// asks that the blob never turns further than square, at any fade setting.
TEST (CrossingWindow, TheBlobNeverTurnsBackOnItselfAtTheSeam)
{
  auto const pattern = aPatternOf (anOpenCircle (48));

  for (auto const reach : { 0.f, 0.25f, 0.5f, 0.75f, 1.f })
    {
      pattern->setFadeReach (reach);
      EXPECT_GE (worstTurnAcross (*pattern, 36.0, 60.0, 0.0625), 0.f)
          << "fade reach " << reach;
    }
}
