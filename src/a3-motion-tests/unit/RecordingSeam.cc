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
#include <a3-motion-engine/TrajectoryShape.hh>
#include <a3-motion-engine/PatternFile.hh>

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

// ── the seam is a playback setting, not a recording one ─────────────────
//
// Baking it in at the end of a take means it can never be changed again. The
// pattern therefore remembers where its seam is, and the two positions at
// either end of it are real ticks somebody played — so it can be filled
// either way, at any time, as often as you like.

TEST (SeamMode, APatternRemembersWhereItsSeamIs)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));

  closeRecordingSeams (pattern, SeamMode::Hard);

  EXPECT_EQ (pattern.getSeamSpan ().begin, 9u);
  EXPECT_EQ (pattern.getSeamSpan ().length, 11u) << "9..15 and 0..3";
}

TEST (SeamMode, SwitchingToGlideAfterwardsSmoothsTheSeam)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));
  closeRecordingSeams (pattern, SeamMode::Hard);

  applySeamMode (pattern, SeamMode::Glide);

  EXPECT_GT (pattern.getTick (14).x (), 0.2f);
  EXPECT_LT (pattern.getTick (14).x (), 3.8f);
}

TEST (SeamMode, SwitchingBackToHardRestoresTheJump)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));
  closeRecordingSeams (pattern, SeamMode::Glide);

  applySeamMode (pattern, SeamMode::Hard);

  EXPECT_NEAR (pattern.getTick (14).x (), 4.f, 0.001f);
}

// Switching back and forth must not drift: each fill starts from the two
// played positions, never from the last fill.
TEST (SeamMode, SwitchingBackAndForthIsStable)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));
  closeRecordingSeams (pattern, SeamMode::Hard);

  for (int round = 0; round < 5; ++round)
    {
      applySeamMode (pattern, SeamMode::Glide);
      applySeamMode (pattern, SeamMode::Hard);
    }

  EXPECT_NEAR (pattern.getTick (14).x (), 4.f, 0.001f);
  EXPECT_NEAR (pattern.getTick (4).x (), 0.f, 0.001f) << "played ticks untouched";
  EXPECT_NEAR (pattern.getTick (8).x (), 4.f, 0.001f);
}

// The stretches somebody played a jump into are not the seam and must stay put
// however often the setting is turned.
TEST (SeamMode, MiddleSpansAreNeverTouchedAgain)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));
  pattern.setTick (8, at (2.f));
  closeRecordingSeams (pattern, SeamMode::Hard);

  applySeamMode (pattern, SeamMode::Glide);

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f);
  EXPECT_NEAR (pattern.getTick (6).x (), 1.f, 0.001f);
}

TEST (SeamMode, APatternWithoutASeamIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);
  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    pattern.setTick (tick, at (0.25f));
  closeRecordingSeams (pattern, SeamMode::Hard);

  applySeamMode (pattern, SeamMode::Glide);   // darf nicht abstuerzen

  EXPECT_EQ (pattern.getSeamSpan ().length, 0u);
  EXPECT_NEAR (pattern.getTick (8).x (), 0.25f, 0.001f);
}

// ── the seam has to survive a restart ───────────────────────────────────
//
// Where a take's seam lies is not something that can be worked out again from
// the file: once it is filled, the stretch looks like any other run of ticks.
// If it is not written down, a restart leaves whatever fill was last applied
// and no way back.

TEST (SeamMode, TheSeamSurvivesASaveAndLoad)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-test-seam.svg");
  file.deleteFile ();

  // 128 ticks to the beat, so a pattern shorter than that saves as zero beats
  // and will not load again.
  auto written = std::make_shared<Pattern> ();
  written->resize (512);
  written->setTick (100, at (0.f));
  written->setTick (200, at (4.f));
  closeRecordingSeams (*written, SeamMode::Hard);
  auto const expected = written->getSeamSpan ();
  ASSERT_GT (expected.length, 0u);

  ASSERT_TRUE (PatternFile::save (written, file));
  auto const loaded = PatternFile::load (file);
  ASSERT_NE (loaded, nullptr);

  EXPECT_EQ (loaded->getSeamSpan ().begin, expected.begin);
  EXPECT_EQ (loaded->getSeamSpan ().length, expected.length);

  file.deleteFile ();
}

// A pattern that never had a seam — a shipped shape, or a take that filled its
// whole loop — must not acquire one from a file that does not mention it.
TEST (SeamMode, AFileWithoutASeamLoadsWithoutOne)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-motion-test-noseam.svg");
  file.deleteFile ();

  auto written = std::make_shared<Pattern> ();
  written->resize (512);
  for (index_t tick = 0; tick < written->getNumTicks (); ++tick)
    written->setTick (tick, at (0.25f));

  ASSERT_TRUE (PatternFile::save (written, file));
  auto const loaded = PatternFile::load (file);
  ASSERT_NE (loaded, nullptr);

  EXPECT_EQ (loaded->getSeamSpan ().length, 0u);

  file.deleteFile ();
}

// Recording in Loop runs several passes, so the loop point gets written from
// two different ones: the last tick carries an early pass and the first a late
// one. There is no unwritten stretch there to fill -- it is an edge between two
// written ticks -- and the seam, which only ever filled holes, had nothing to
// do. The blob snapped from the end back to the start.

namespace
{
std::vector<Pos> circleWithAGapAtTheLoopPoint (int numTicks = 64)
{
  // Three quarters of a circle, evenly stepped, ending nowhere near where it
  // began: exactly the edge a second pass leaves behind.
  std::vector<Pos> ticks;
  for (int i = 0; i < numTicks; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi * 0.75f
                     * static_cast<float> (i) / static_cast<float> (numTicks);
      ticks.push_back (
          Pos::fromCartesian (std::cos (a) * 0.6f, std::sin (a) * 0.6f, 0.f));
    }
  return ticks;
}

// Pattern holds an atomic status, so it neither copies nor moves: filled in
// place rather than returned.
void writeThrough (Pattern &pattern, std::vector<Pos> const &ticks)
{
  pattern.resize (static_cast<index_t> (ticks.size ()));
  for (index_t tick = 0; tick < ticks.size (); ++tick)
    pattern.setTick (tick, ticks[tick]);
}

float gapAtLoopPoint (Pattern const &pattern)
{
  auto const last = pattern.getTick (pattern.getNumTicks () - 1);
  auto const first = pattern.getTick (0);
  return std::sqrt (std::pow (last.x () - first.x (), 2.f)
                    + std::pow (last.y () - first.y (), 2.f)
                    + std::pow (last.z () - first.z (), 2.f));
}
}

TEST (RecordingSeam, TheLoopPointIsClosedEvenWhenBothSidesWereWritten)
{
  auto const ticks = circleWithAGapAtTheLoopPoint ();
  Pattern pattern;
  writeThrough (pattern, ticks);
  auto const before = gapAtLoopPoint (pattern);

  closeRecordingSeams (pattern, SeamMode::Glide);

  auto const step = typicalTrajectoryStep (ticks);
  EXPECT_GT (before, trajectoryJumpThreshold (step))
      << "the fixture has to start with a jump or this proves nothing";
  EXPECT_LT (gapAtLoopPoint (pattern), trajectoryJumpThreshold (step));
}

// And it takes as long as the motion itself would: the closing arc moves at
// the trajectory's own speed, so it does not read as a dash across the sphere.
TEST (RecordingSeam, TheClosingGlideMovesAtTheTrajectorysOwnSpeed)
{
  auto const ticks = circleWithAGapAtTheLoopPoint ();
  Pattern pattern;
  writeThrough (pattern, ticks);

  closeRecordingSeams (pattern, SeamMode::Glide);

  auto const step = typicalTrajectoryStep (ticks);
  auto const threshold = trajectoryJumpThreshold (step);
  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    {
      auto const here = pattern.getTick (tick);
      auto const next = pattern.getTick ((tick + 1) % pattern.getNumTicks ());
      auto const hop = std::sqrt (std::pow (here.x () - next.x (), 2.f)
                                  + std::pow (here.y () - next.y (), 2.f));
      EXPECT_LT (hop, threshold) << "tick " << tick;
    }
}

TEST (RecordingSeam, HardLeavesTheLoopPointAsItWasPlayed)
{
  auto const ticks = circleWithAGapAtTheLoopPoint ();
  Pattern pattern;
  writeThrough (pattern, ticks);
  auto const before = gapAtLoopPoint (pattern);

  closeRecordingSeams (pattern, SeamMode::Hard);

  EXPECT_NEAR (gapAtLoopPoint (pattern), before, 0.001f);
}

// A pass that already comes round to where it started is not touched -- there
// is nothing to close, and overwriting its tail would be vandalism.
TEST (RecordingSeam, AnAlreadyClosedLoopKeepsItsTail)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < 64; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi * i / 64.f;
      ticks.push_back (
          Pos::fromCartesian (std::cos (a) * 0.6f, std::sin (a) * 0.6f, 0.f));
    }
  Pattern pattern;
  writeThrough (pattern, ticks);

  closeRecordingSeams (pattern, SeamMode::Glide);

  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), ticks[tick].x (), 0.0001f)
        << "tick " << tick;
}
