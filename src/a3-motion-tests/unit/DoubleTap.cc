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

#include <a3-motion-ui/components/DoubleTap.hh>

using namespace a3;

namespace
{
TapTouch
tap (juce::int64 downMs, juce::int64 upMs, int x = 100, int y = 100,
     bool moved = false)
{
  return { downMs, upMs, { x, y }, moved };
}
}

TEST (DoubleTap, TwoStillTapsCloseTogetherAreADoubleTap)
{
  EXPECT_TRUE (isDoubleTap (tap (0, 80), tap (200, 280),
                            DoubleTapMovement::MustBeStill));
}

TEST (DoubleTap, TooSlowIsTwoTaps)
{
  EXPECT_FALSE (isDoubleTap (tap (0, 80), tap (500, 580),
                             DoubleTapMovement::MustBeStill));
}

TEST (DoubleTap, TooFarApartIsTwoTaps)
{
  EXPECT_FALSE (isDoubleTap (tap (0, 80), tap (200, 280, 100 + doubleTapSlopPx, 100),
                             DoubleTapMovement::MustBeStill));
}

TEST (DoubleTap, NothingBeforeItIsNoDoubleTap)
{
  EXPECT_FALSE (isDoubleTap ({}, tap (200, 280), DoubleTapMovement::MayMove));
}

// Where a control asks for stillness, a wobble that stepped the value is a
// drag, as it always was.
TEST (DoubleTap, AStillControlTakesAMovedTouchForADrag)
{
  auto const wobbled = tap (200, 280, 100, 100, true);

  EXPECT_FALSE (countsAsTap (wobbled, DoubleTapMovement::MustBeStill));
  EXPECT_FALSE (isDoubleTap (tap (0, 80), wobbled,
                             DoubleTapMovement::MustBeStill));
}

// The meter: two fast taps that wobbled are still a double tap.
TEST (DoubleTap, OnTheMeterAWobbleDoesNotSpoilIt)
{
  EXPECT_TRUE (isDoubleTap (tap (0, 80, 100, 100, true),
                            tap (200, 280, 104, 110, true),
                            DoubleTapMovement::MayMove));
}

// But a long touch that moved is a drag even there: pull VOL down, put the
// finger straight back to pull further, and letting go must not throw the
// channel to full.
TEST (DoubleTap, OnTheMeterADragPickedBackUpIsNotADoubleTap)
{
  auto const drag = tap (0, 900, 100, 100, true);
  auto const moreDrag = tap (1000, 1250 + shortTouchMs, 100, 100, true);

  EXPECT_FALSE (countsAsTap (drag, DoubleTapMovement::MayMove));
  EXPECT_FALSE (isDoubleTap (tap (0, 80), moreDrag, DoubleTapMovement::MayMove));
}

TEST (DoubleTap, OnTheMeterAShortTouchCountsAsATapMovedOrNot)
{
  EXPECT_TRUE (countsAsTap (tap (0, 100, 0, 0, true), DoubleTapMovement::MayMove));
  EXPECT_TRUE (countsAsTap (tap (0, 100), DoubleTapMovement::MayMove));
  EXPECT_TRUE (countsAsTap (tap (0, 100), DoubleTapMovement::MustBeStill));
}
