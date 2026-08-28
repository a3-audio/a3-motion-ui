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

#include <a3-motion-engine/RecordingSpans.hh>

using namespace a3;

// A recording writes only where the finger was down. What it never wrote has to
// be found before it can be filled — and the pattern is a ring, so the stretch
// that runs past the loop point is one stretch, not two. Splitting it would
// fill the seam twice and hand the start an interpolation leading nowhere.

TEST (RecordingSpans, NothingUnwrittenWhenEverythingWasWritten)
{
  EXPECT_TRUE (unwrittenSpans ({ true, true, true, true }).empty ());
}

TEST (RecordingSpans, AllOfItWhenNothingWasWritten)
{
  auto const spans = unwrittenSpans ({ false, false, false, false });

  ASSERT_EQ (spans.size (), 1u);
  EXPECT_EQ (spans[0].begin, 0u);
  EXPECT_EQ (spans[0].length, 4u);
}

TEST (RecordingSpans, ASpanInTheMiddle)
{
  auto const spans = unwrittenSpans ({ true, false, false, true });

  ASSERT_EQ (spans.size (), 1u);
  EXPECT_EQ (spans[0].begin, 1u);
  EXPECT_EQ (spans[0].length, 2u);
}

TEST (RecordingSpans, TwoSeparateSpans)
{
  auto const spans = unwrittenSpans ({ true, false, true, false, true, true });

  ASSERT_EQ (spans.size (), 2u);
  EXPECT_EQ (spans[0].begin, 1u);
  EXPECT_EQ (spans[0].length, 1u);
  EXPECT_EQ (spans[1].begin, 3u);
  EXPECT_EQ (spans[1].length, 1u);
}

TEST (RecordingSpans, ASpanCrossingTheLoopPointIsOneSpan)
{
  auto const spans = unwrittenSpans ({ false, true, true, false, false });

  ASSERT_EQ (spans.size (), 1u);
  EXPECT_EQ (spans[0].begin, 3u) << "begins near the end";
  EXPECT_EQ (spans[0].length, 3u) << "and runs across the loop point";
}

TEST (RecordingSpans, ASingleWrittenTickLeavesOneSpanAroundIt)
{
  auto const spans = unwrittenSpans ({ false, true, false, false });

  ASSERT_EQ (spans.size (), 1u);
  EXPECT_EQ (spans[0].begin, 2u);
  EXPECT_EQ (spans[0].length, 3u);
}

TEST (RecordingSpans, AnEmptyPatternHasNoSpans)
{
  EXPECT_TRUE (unwrittenSpans ({}).empty ());
}
