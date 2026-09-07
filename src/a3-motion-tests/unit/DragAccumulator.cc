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

#include <a3-motion-ui/components/DragAccumulator.hh>

using namespace a3;

TEST (DragAccumulator, BelowThresholdYieldsNothing)
{
  DragAccumulator acc{ 12 };
  EXPECT_EQ (acc.stepsFor (11), 0);
  EXPECT_EQ (acc.stepsFor (-11), 0);
}

TEST (DragAccumulator, ThresholdYieldsExactlyOneStep)
{
  DragAccumulator acc{ 12 };
  EXPECT_EQ (acc.stepsFor (12), 1);
}

TEST (DragAccumulator, NegativeMovementYieldsNegativeStep)
{
  DragAccumulator acc{ 12 };
  EXPECT_EQ (acc.stepsFor (-12), -1);
}

// What a fast drag does: JUCE coalesces movement, so a single event can
// cross several steps at once. Emitting one step per event loses the rest.
TEST (DragAccumulator, OneBigJumpYieldsEveryStepItCrossed)
{
  DragAccumulator acc{ 12 };
  EXPECT_EQ (acc.stepsFor (60), 5);
}

// Reports are absolute, not relative: the second one already contains the
// first.
TEST (DragAccumulator, ReportsAreAbsoluteNotRelative)
{
  DragAccumulator acc{ 12 };
  EXPECT_EQ (acc.stepsFor (12), 1);
  EXPECT_EQ (acc.stepsFor (24), 1);
  EXPECT_EQ (acc.stepsFor (24), 0);
  EXPECT_EQ (acc.emittedSteps (), 2);
}

// Up and back to where it started cancels out — otherwise a value drifts
// while a finger pushes it to and fro.
TEST (DragAccumulator, ReturningToStartCancelsOut)
{
  DragAccumulator acc{ 12 };
  EXPECT_EQ (acc.stepsFor (36), 3);
  EXPECT_EQ (acc.stepsFor (0), -3);
  EXPECT_EQ (acc.emittedSteps (), 0);
}

TEST (DragAccumulator, ResetForgetsTheDrag)
{
  DragAccumulator acc{ 12 };
  acc.stepsFor (36);
  acc.reset ();
  EXPECT_EQ (acc.emittedSteps (), 0);
  EXPECT_EQ (acc.stepsFor (12), 1);
}

// A nonsensical threshold must not divide by zero.
TEST (DragAccumulator, ZeroPixelsPerStepIsClampedNotDividedBy)
{
  DragAccumulator acc{ 0 };
  EXPECT_EQ (acc.stepsFor (1), 1);
}

// ── Picking a drag back up ───────────────────────────────────────────────

/** A screen has an edge, and the controls near it are the ones with the least
 *  room to drag. Run a finger off the bottom of the panel and the touch ends;
 *  put it back and, as it stood, the next press was a fresh gesture -- which
 *  on a key that also taps meant the value you had just dragged to was thrown
 *  away and replaced by whatever the key under your finger says.
 *
 *  So a press that lands soon after a drag on the same control carries on
 *  from it: no tap, and the steps already emitted are remembered so the
 *  release knows a drag is what this was.
 */
TEST (DragAccumulator, ADragPickedBackUpCarriesOnFromWhereItStopped)
{
  DragAccumulator drag{ 12 };

  EXPECT_EQ (drag.stepsFor (-36), -3);
  EXPECT_EQ (drag.emittedSteps (), -3);

  // The finger left at the screen's edge and came back. The new gesture is
  // told it carries one on, so the release that follows knows this was a drag
  // and not a tap -- but the pixels count from where the finger went down.
  DragAccumulator resumed{ 12 };
  resumed.resume ();

  EXPECT_TRUE (resumed.hasMoved ()) << "picked back up, it is still a drag";
  EXPECT_EQ (resumed.stepsFor (-12), -1)
      << "the second half of the drag counts from where the finger went down";
  EXPECT_TRUE (resumed.hasMoved ());
}

// And a gesture that was not picked up from anything is a tap until it moves.
TEST (DragAccumulator, AFreshGestureHasNotMovedUntilItDoes)
{
  DragAccumulator drag{ 12 };

  EXPECT_FALSE (drag.hasMoved ());
  drag.stepsFor (-4);
  EXPECT_FALSE (drag.hasMoved ()) << "four pixels is a finger resting";
  drag.stepsFor (-24);
  EXPECT_TRUE (drag.hasMoved ());
}
