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

#include <a3-motion-engine/RecordingSeam.hh>

using namespace a3;

namespace
{
Pos at (float x) { return Pos::fromCartesian (x, 0.f, 0.f); }
}

// A recording writes only where the finger was down, so what it leaves behind
// has gaps. Both kinds — a hole in the middle and the stretch between the last
// thing written and the first — are the same thing, and one rule fills them.

TEST (RecordingSeam, GlideFillsAMiddleSpanByInterpolating)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));

  closeRecordingSeams (pattern, SeamMode::Glide);

  EXPECT_NEAR (pattern.getTick (2).x (), 0.5f, 0.02f);
}

TEST (RecordingSeam, HardHoldsTheLastWrittenPosition)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));

  closeRecordingSeams (pattern, SeamMode::Hard);

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f) << "held, then a jump";
}

// The case the maintainer asked for: almost a full pass recorded, and the end
// has to meet the start again or the loop point is audible as a jump.
TEST (RecordingSeam, GlideLeavesNoTickUnfilledAcrossTheLoopPoint)
{
  Pattern pattern;
  pattern.resize (16);
  auto const numTicks = pattern.getNumTicks ();
  ASSERT_GT (numTicks, 4u);

  for (index_t tick = 0; tick + 2 < numTicks; ++tick)
    pattern.setTick (tick, at (static_cast<float> (tick) / numTicks));

  closeRecordingSeams (pattern, SeamMode::Glide);

  for (index_t tick = 0; tick < numTicks; ++tick)
    EXPECT_TRUE (pattern.getTick (tick).isValid ()) << "tick " << tick;
}

TEST (RecordingSeam, AFullyWrittenPatternIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);
  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    pattern.setTick (tick, at (0.25f));

  closeRecordingSeams (pattern, SeamMode::Glide);

  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), 0.25f, 0.001f);
}

TEST (RecordingSeam, APatternThatWroteNothingIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);

  closeRecordingSeams (pattern, SeamMode::Glide);

  EXPECT_FALSE (pattern.getTick (0).isValid ())
      << "nothing to interpolate between, so nothing invented";
}

// A single written tick is the degenerate case: the stretch wraps all the way
// round back to it, so both ends of the interpolation are the same position.
TEST (RecordingSeam, ASingleWrittenTickFillsTheWholePattern)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (3, at (0.7f));

  closeRecordingSeams (pattern, SeamMode::Glide);

  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), 0.7f, 0.001f) << "tick " << tick;
}
