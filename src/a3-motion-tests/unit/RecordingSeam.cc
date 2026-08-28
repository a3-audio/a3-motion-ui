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

  closeRecordingSeams (pattern, index_t{ 64 });

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f)
      << "the finger lifted here on purpose - that is a jump somebody played";
}

TEST (RecordingSeam, AMiddleSpanIsHeldWhenHard)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));

  closeRecordingSeams (pattern, index_t{ 0 });

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

  closeRecordingSeams (pattern, index_t{ 64 });

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

  closeRecordingSeams (pattern, index_t{ 64 });

  // From tick 8 round to tick 4: twelve ticks, so tick 14 is halfway-ish and
  // must be somewhere between the two, not sitting on either.
  EXPECT_GT (pattern.getTick (14).x (), 0.2f);
  EXPECT_LT (pattern.getTick (14).x (), 3.8f);
}

TEST (RecordingSeam, TheSpanAcrossTheLoopPointHoldsWithoutAFade)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));

  closeRecordingSeams (pattern, index_t{ 0 });

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

  closeRecordingSeams (pattern, index_t{ 64 });

  for (index_t tick = 0; tick < numTicks; ++tick)
    EXPECT_TRUE (pattern.getTick (tick).isValid ()) << "tick " << tick;
}

TEST (RecordingSeam, AFullyWrittenPatternIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);
  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    pattern.setTick (tick, at (0.25f));

  closeRecordingSeams (pattern, index_t{ 64 });

  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), 0.25f, 0.001f);
}

TEST (RecordingSeam, APatternThatWroteNothingIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);

  closeRecordingSeams (pattern, index_t{ 64 });

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

  closeRecordingSeams (pattern, index_t{ 64 });

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

  closeRecordingSeams (pattern, index_t{ 64 });

  EXPECT_EQ (pattern.getLastUpdatedTick (), pattern.getNumTicks () - 1)
      << "otherwise playback squeezes the whole take into a fraction of it";
}

TEST (RecordingSeam, APatternThatWroteNothingKeepsItsLength)
{
  Pattern pattern;
  pattern.resize (16);

  closeRecordingSeams (pattern, index_t{ 64 });

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

  closeRecordingSeams (pattern, index_t{ 0 });

  EXPECT_EQ (pattern.getSeamSpan ().begin, 9u);
  EXPECT_EQ (pattern.getSeamSpan ().length, 11u) << "9..15 and 0..3";
}

TEST (SeamMode, SwitchingToGlideAfterwardsSmoothsTheSeam)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));
  closeRecordingSeams (pattern, index_t{ 0 });

  applyFade (pattern, index_t{ 64 });

  EXPECT_GT (pattern.getTick (14).x (), 0.2f);
  EXPECT_LT (pattern.getTick (14).x (), 3.8f);
}

TEST (SeamMode, SwitchingBackToHardRestoresTheJump)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));
  closeRecordingSeams (pattern, index_t{ 64 });

  applyFade (pattern, index_t{ 0 });

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
  closeRecordingSeams (pattern, index_t{ 0 });

  for (int round = 0; round < 5; ++round)
    {
      applyFade (pattern, index_t{ 64 });
      applyFade (pattern, index_t{ 0 });
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
  closeRecordingSeams (pattern, index_t{ 0 });

  applyFade (pattern, index_t{ 64 });

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f);
  EXPECT_NEAR (pattern.getTick (6).x (), 1.f, 0.001f);
}

TEST (SeamMode, APatternWithoutASeamIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);
  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    pattern.setTick (tick, at (0.25f));
  closeRecordingSeams (pattern, index_t{ 0 });

  applyFade (pattern, index_t{ 64 });   // darf nicht abstuerzen

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
  closeRecordingSeams (*written, index_t{ 0 });
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

  closeRecordingSeams (pattern, index_t{ 64 });

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

  closeRecordingSeams (pattern, index_t{ 64 });

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

TEST (RecordingSeam, AFadeOfZeroLeavesTheJoinAsItWasPlayed)
{
  auto const ticks = circleWithAGapAtTheLoopPoint ();
  Pattern pattern;
  writeThrough (pattern, ticks);
  auto const before = gapAtLoopPoint (pattern);

  closeRecordingSeams (pattern, index_t{ 0 });

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

  closeRecordingSeams (pattern, index_t{ 64 });

  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), ticks[tick].x (), 0.0001f)
        << "tick " << tick;
}

// What the device actually records. Ticks run at about 277 Hz and a finger
// reports at 60 to 120, so most ticks repeat the position of the one before
// them. More than half of all steps are therefore exactly zero, and their
// median -- the trajectory's "typical step" -- is zero too. True, and useless
// as a speed: measured against it the closing glide could not be sized, and
// the loop point was left open on every real take.
TEST (RecordingSeam, ALoopPointClosesEvenWhenMostTicksRepeat)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < 256; ++i)
    {
      // Three quarters of a circle, but each position held for four ticks.
      auto const a = juce::MathConstants<float>::twoPi * 0.75f
                     * static_cast<float> (i / 4) / 64.f;
      ticks.push_back (
          Pos::fromCartesian (std::cos (a) * 0.6f, std::sin (a) * 0.6f, 0.f));
    }

  ASSERT_FLOAT_EQ (typicalTrajectoryStep (ticks), 0.f)
      << "the fixture has to have a zero median or it proves nothing";
  ASSERT_GT (typicalTrajectorySpeed (ticks), 0.f);

  Pattern pattern;
  writeThrough (pattern, ticks);
  auto const before = gapAtLoopPoint (pattern);

  closeRecordingSeams (pattern, index_t{ 64 });

  EXPECT_GT (before, 0.5f);
  EXPECT_LT (gapAtLoopPoint (pattern), 0.2f)
      << "the loop point was left open because the median step is zero";
}

// The speed only counts the ticks that moved.
TEST (RecordingSeam, SpeedIgnoresHeldTicks)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < 100; ++i)
    ticks.push_back (Pos::fromCartesian (0.1f * (i / 10), 0.f, 0.f));

  EXPECT_FLOAT_EQ (typicalTrajectoryStep (ticks), 0.f);
  EXPECT_NEAR (typicalTrajectorySpeed (ticks), 0.1f, 0.001f);
}

// Recording in Loop runs several passes over the same ticks, so where it stops
// is an edge: the ticks up to it carry the freshest pass and the ones after it
// still carry the one before. Nothing is missing there -- 512 of 512 ticks
// written, no gaps at all on a real take -- so filling holes never reached it,
// and the seam sat at the loop point where there was nothing wrong.
//
// Measured on a real take: one step of 2.01 between two written neighbours,
// against a 90th percentile of 0.105.
TEST (RecordingSeam, TheSeamSitsWhereTheTakeStopped)
{
  // A full circle, then a second pass over the first quarter that ends
  // somewhere else entirely: the edge is at tick 63, not at the loop point.
  std::vector<Pos> ticks;
  for (int i = 0; i < 256; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi * i / 256.f;
      ticks.push_back (
          Pos::fromCartesian (std::cos (a) * 0.6f, std::sin (a) * 0.6f, 0.f));
    }
  for (int i = 0; i < 64; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi * i / 256.f;
      ticks[static_cast<size_t> (i)] = Pos::fromCartesian (
          std::cos (a) * 0.6f - 1.2f, std::sin (a) * 0.6f, 0.f);
    }

  auto const edge = [] (Pattern const &p) {
    auto const a = p.getTick (63);
    auto const b = p.getTick (64);
    return std::sqrt (std::pow (a.x () - b.x (), 2.f)
                      + std::pow (a.y () - b.y (), 2.f));
  };

  Pattern pattern;
  writeThrough (pattern, ticks);
  ASSERT_GT (edge (pattern), 1.f) << "the fixture needs the edge it is about";

  closeRecordingSeams (pattern, index_t{ 64 }, index_t{ 63 });

  EXPECT_LT (edge (pattern), 0.3f)
      << "the seam was put at the loop point, where nothing was wrong";
}

// And the fresh pass is what survives: the glide is written over the stale
// material after the edge, never back into what was just played.
TEST (RecordingSeam, TheFreshPassIsNotOverwritten)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < 256; ++i)
    ticks.push_back (Pos::fromCartesian (0.004f * i, 0.f, 0.f));
  for (int i = 200; i < 256; ++i)
    ticks[static_cast<size_t> (i)] = Pos::fromCartesian (-0.9f, 0.5f, 0.f);

  Pattern pattern;
  writeThrough (pattern, ticks);
  closeRecordingSeams (pattern, index_t{ 64 }, index_t{ 199 });

  for (index_t tick = 0; tick <= 199; ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), ticks[tick].x (), 0.0001f)
        << "tick " << tick << " is inside the pass that was just played";
}

// A take is closed when it is recorded, and the file it is written to does not
// keep it that way. The SVG stores a shape, not a recording: the writer cuts
// it into segments at its jumps and drops the tick timing, and the reader
// walks the segments end to end at a constant rate. The path then begins where
// the first segment begins and ends where the last one begins -- two different
// places, while the ticks they came from were the same one.
//
// Measured on a real take: nothing between tick 511 and tick 0 before saving,
// 0.945 after loading it back. The reloaded pattern is what goes into the slot
// and plays, so closing the take is not enough -- what comes back has to be
// closed too.
TEST (RecordingSeam, AReloadedTakeHasItsLoopPointClosedAgain)
{
  auto const ticks = circleWithAGapAtTheLoopPoint (256);

  Pattern pattern;
  writeThrough (pattern, ticks);
  closeRecordingSeams (pattern, index_t{ 64 });
  ASSERT_LT (gapAtLoopPoint (pattern), 0.2f) << "the take itself must close";

  auto shared = std::make_shared<Pattern> ();
  writeThrough (*shared, pattern.getTicks ().positions);
  shared->setName ("Reloaded");
  shared->resize (512); // a length PatternFile can express in whole beats
  for (index_t tick = 0; tick < 512; ++tick)
    shared->setTick (tick, pattern.getTick (tick % pattern.getNumTicks ()));

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-reloaded-take.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (shared, file));

  auto reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);

  closeRecordingSeams (*reloaded, index_t{ 64 });
  EXPECT_LT (gapAtLoopPoint (*reloaded), 0.3f)
      << "what came back from the file is what plays, and it was left open";

  file.deleteFile ();
}

// The fade is a playback setting, so it has to still take hold after a
// restart. Where the take stopped cannot be worked out from the file again --
// once the closing move is written, that stretch looks like any other run of
// ticks -- so it travels with it.
TEST (SeamMode, TheJoinSurvivesARestart)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setName ("Joined");
  pattern->resize (512);
  for (index_t tick = 0; tick < 512; ++tick)
    {
      auto const a = juce::MathConstants<float>::twoPi * tick / 512.f;
      pattern->setTick (tick, Pos::fromCartesian (std::cos (a) * 0.6f,
                                                  std::sin (a) * 0.6f, 0.f));
    }
  pattern->setSeamJoin (index_t{ 271 });

  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-seam-join.svg");
  file.deleteFile ();
  ASSERT_TRUE (PatternFile::save (pattern, file));

  auto const reloaded = PatternFile::load (file);
  ASSERT_NE (reloaded, nullptr);
  ASSERT_TRUE (reloaded->getSeamJoin ().has_value ());
  EXPECT_EQ (*reloaded->getSeamJoin (), 271u);

  file.deleteFile ();
}

// The fade used to be written straight into the ticks, which made it a one-way
// door: lengthening read its far end from material nobody had touched and
// worked, shortening read from the previous fill -- a point already on the old
// closing move -- and changed nothing anyone could see.
TEST (SeamMode, ShorteningTheFadeWorksAsWellAsLengtheningIt)
{
  auto const ticks = circleWithAGapAtTheLoopPoint (256);
  Pattern pattern;
  writeThrough (pattern, ticks);
  closeRecordingSeams (pattern, index_t{ 64 }, index_t{ 200 });

  auto const spanOf = [&pattern] (index_t fade) {
    applyFade (pattern, fade);
    // How far from the take as played the pattern now reads: a longer closing
    // move touches more ticks, a shorter one fewer.
    int touched = 0;
    auto const baseline = pattern.getFadeBaseline ();
    for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
      if (std::abs (pattern.getTick (tick).x () - baseline[tick].x ()) > 1e-5f
          || std::abs (pattern.getTick (tick).y () - baseline[tick].y ()) > 1e-5f)
        ++touched;
    return touched;
  };

  auto const wide = spanOf (48);
  auto const narrow = spanOf (16);
  auto const none = spanOf (0);

  EXPECT_GT (wide, narrow) << "shortening the fade did not shorten anything";
  EXPECT_GT (narrow, none);
  EXPECT_EQ (none, 0) << "turning it off must give the take back as played";

  // And going back up gets there again -- no drift from having been turned.
  EXPECT_EQ (spanOf (48), wide);
}

// The closing move continues the motion rather than cutting across it: laid
// out as a straight line it was visibly two chords over a take that has none.
TEST (SeamMode, TheClosingMoveIsACurveNotAChord)
{
  auto const ticks = circleWithAGapAtTheLoopPoint (256);
  Pattern pattern;
  writeThrough (pattern, ticks);
  closeRecordingSeams (pattern, index_t{ 48 }, index_t{ 200 });

  // Three points across the closing move: a straight line puts the middle one
  // exactly halfway between its neighbours.
  auto const a = pattern.getTick (205);
  auto const b = pattern.getTick (215);
  auto const c = pattern.getTick (225);
  auto const midX = (a.x () + c.x ()) * 0.5f;
  auto const midY = (a.y () + c.y ()) * 0.5f;

  EXPECT_GT (std::hypot (b.x () - midX, b.y () - midY), 0.01f)
      << "the closing move is a straight line";
}

// The curve was a curve on paper and a straight line on the device. Its
// tangents were measured over one tick, and ticks run far faster than a finger
// reports, so the tick before the join repeats the one before it: both
// tangents came out zero, and a cubic with two zero tangents is a straight
// line between its ends -- exactly what it was meant to replace.
TEST (SeamMode, TheCurveSurvivesTicksThatRepeat)
{
  // Every position held for four ticks, as a real take writes them.
  std::vector<Pos> ticks;
  for (int i = 0; i < 256; ++i)
    {
      auto const a = juce::MathConstants<float>::twoPi * (i / 4) / 64.f;
      ticks.push_back (
          Pos::fromCartesian (std::cos (a) * 0.6f, std::sin (a) * 0.6f, 0.f));
    }
  for (int i = 200; i < 256; ++i)
    ticks[static_cast<size_t> (i)]
        = Pos::fromCartesian (-0.8f, -0.4f + 0.004f * (i - 200), 0.f);

  Pattern pattern;
  writeThrough (pattern, ticks);
  closeRecordingSeams (pattern, index_t{ 48 }, index_t{ 199 });

  auto const a = pattern.getTick (205);
  auto const b = pattern.getTick (223);
  auto const c = pattern.getTick (241);
  auto const midX = (a.x () + c.x ()) * 0.5f;
  auto const midY = (a.y () + c.y ()) * 0.5f;

  EXPECT_GT (std::hypot (b.x () - midX, b.y () - midY), 0.01f)
      << "two zero tangents made the closing move a straight line again";
}
