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
// has gaps — but the two kinds are not the same thing.
//
// A finger lifted mid-take lifted on purpose: that is a jump somebody played,
// and smoothing it would erase what they did. It is always held and then
// jumped.
//
// The stretch across the loop point is different. Nobody played it; it is
// where the take happens to have started and stopped, and a jump there is an
// artefact. That one, and only that one, the clip's seam setting decides.

TEST (RecordingSeam, AMiddleSpanIsHeldEvenWhenGliding)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));

  closeRecordingSeams (pattern, SeamMode::Glide);

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f)
      << "the finger lifted here on purpose - that is a jump somebody played";
}

TEST (RecordingSeam, AMiddleSpanIsHeldWhenHard)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));

  closeRecordingSeams (pattern, SeamMode::Hard);

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f);
}

// Tapping four positions is the case this is for: hold each one, jump to the
// next, rather than touring between them.
TEST (RecordingSeam, TappedPositionsAreHeldNotToured)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));
  pattern.setTick (8, at (2.f));

  closeRecordingSeams (pattern, SeamMode::Glide);

  EXPECT_NEAR (pattern.getTick (3).x (), 0.f, 0.001f);
  EXPECT_NEAR (pattern.getTick (7).x (), 1.f, 0.001f);
}

// The one span the setting decides: the take's own seam.
TEST (RecordingSeam, TheSpanAcrossTheLoopPointGlides)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));

  closeRecordingSeams (pattern, SeamMode::Glide);

  // From tick 8 round to tick 4: twelve ticks, so tick 14 is halfway-ish and
  // must be somewhere between the two, not sitting on either.
  EXPECT_GT (pattern.getTick (14).x (), 0.2f);
  EXPECT_LT (pattern.getTick (14).x (), 3.8f);
}

TEST (RecordingSeam, TheSpanAcrossTheLoopPointHoldsWhenHard)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));

  closeRecordingSeams (pattern, SeamMode::Hard);

  EXPECT_NEAR (pattern.getTick (14).x (), 4.f, 0.001f)
      << "held at the last thing played, then a jump at the loop point";
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

// Playback takes _lastUpdatedTick + 1 as the pattern's effective length — a
// leftover from when a take only ever filled a prefix. Filling the spans
// writes the one across the loop point last, and that one ends at a low tick
// number, so the whole pattern was played inside those few ticks: four tapped
// positions came out in a rush. A pattern whose every tick has been filled has
// to say so.
TEST (RecordingSeam, AFilledPatternReportsItsFullLength)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (2, Pos::fromCartesian (0.f, 0.f, 0.f));
  pattern.setTick (9, Pos::fromCartesian (1.f, 0.f, 0.f));

  closeRecordingSeams (pattern, SeamMode::Glide);

  EXPECT_EQ (pattern.getLastUpdatedTick (), pattern.getNumTicks () - 1)
      << "otherwise playback squeezes the whole take into a fraction of it";
}

TEST (RecordingSeam, APatternThatWroteNothingKeepsItsLength)
{
  Pattern pattern;
  pattern.resize (16);

  closeRecordingSeams (pattern, SeamMode::Glide);

  EXPECT_EQ (pattern.getLastUpdatedTick (), 0u) << "nothing was filled";
}
