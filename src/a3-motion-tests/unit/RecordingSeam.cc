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

  closeRecordingSeams (pattern);

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f)
      << "the finger lifted here on purpose - that is a jump somebody played";
}

TEST (RecordingSeam, AMiddleSpanIsHeldWhenHard)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));

  closeRecordingSeams (pattern);

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

  closeRecordingSeams (pattern);

  EXPECT_NEAR (pattern.getTick (3).x (), 0.f, 0.001f);
  EXPECT_NEAR (pattern.getTick (7).x (), 1.f, 0.001f);
}


TEST (RecordingSeam, TheSpanAcrossTheLoopPointHoldsWithoutAFade)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));

  closeRecordingSeams (pattern);

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

  closeRecordingSeams (pattern);

  for (index_t tick = 0; tick < numTicks; ++tick)
    EXPECT_TRUE (pattern.getTick (tick).isValid ()) << "tick " << tick;
}

TEST (RecordingSeam, AFullyWrittenPatternIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);
  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    pattern.setTick (tick, at (0.25f));

  closeRecordingSeams (pattern);

  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), 0.25f, 0.001f);
}

TEST (RecordingSeam, APatternThatWroteNothingIsLeftAlone)
{
  Pattern pattern;
  pattern.resize (16);

  closeRecordingSeams (pattern);

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

  closeRecordingSeams (pattern);

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

  closeRecordingSeams (pattern);

  EXPECT_EQ (pattern.getLastUpdatedTick (), pattern.getNumTicks () - 1)
      << "otherwise playback squeezes the whole take into a fraction of it";
}

TEST (RecordingSeam, APatternThatWroteNothingKeepsItsLength)
{
  Pattern pattern;
  pattern.resize (16);

  closeRecordingSeams (pattern);

  EXPECT_EQ (pattern.getLastUpdatedTick (), 0u) << "nothing was filled";
}

// ── the seam is a gap like any other ────────────────────────────────────
//
// It used to be filled with a computed closing move, written straight into the
// ticks. That made the fade a one-way door -- the take was changed, and the
// only way back was a second copy of it kept beside the first. The seam is now
// held like every other hole, and whether it is drawn through is read from the
// clip at playback. Nothing writes over a take any more.







// ── the seam has to survive a restart ───────────────────────────────────
//
// Where a take's seam lies is not something that can be worked out again from
// the file: once it is filled, the stretch looks like any other run of ticks.
// If it is not written down, a restart leaves whatever fill was last applied
// and no way back.



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






// The speed only counts the ticks that moved.
TEST (RecordingSeam, SpeedIgnoresHeldTicks)
{
  std::vector<Pos> ticks;
  for (int i = 0; i < 100; ++i)
    ticks.push_back (Pos::fromCartesian (0.1f * (i / 10), 0.f, 0.f));

  EXPECT_FLOAT_EQ (typicalTrajectoryStep (ticks), 0.f);
  EXPECT_NEAR (typicalTrajectorySpeed (ticks), 0.1f, 0.001f);
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
  closeRecordingSeams (pattern, index_t{ 199 });

  for (index_t tick = 0; tick <= 199; ++tick)
    EXPECT_NEAR (pattern.getTick (tick).x (), ticks[tick].x (), 0.0001f)
        << "tick " << tick << " is inside the pass that was just played";
}

// ── what replaces all of that ───────────────────────────────────────────

// The strongest claim the new model can make, and the one the old machinery
// needed a whole second copy of the take to approximate: turning the fade does
// not touch the take at all. There is no drift to test for because there is
// nothing to drift.
TEST (SeamIsAGapLikeAnyOther, TurningTheFadeNeverChangesATick)
{
  auto const ticks = circleWithAGapAtTheLoopPoint (256);
  Pattern pattern;
  writeThrough (pattern, ticks);
  closeRecordingSeams (pattern, index_t{ 200 });

  auto const asPlayed = pattern.getTicks ().positions;

  for (int round = 0; round < 5; ++round)
    {
      pattern.setFadeReach (1.f);
      pattern.setFadeReach (0.f);
      pattern.setFadeReach (0.5f);
    }

  auto const afterwards = pattern.getTicks ().positions;
  ASSERT_EQ (asPlayed.size (), afterwards.size ());
  for (size_t tick = 0; tick < asPlayed.size (); ++tick)
    {
      EXPECT_FLOAT_EQ (asPlayed[tick].x (), afterwards[tick].x ())
          << "tick " << tick << " moved";
      EXPECT_FLOAT_EQ (asPlayed[tick].y (), afterwards[tick].y ());
    }
}

// The take's own join is held, exactly as played -- no travel is written into
// it. What used to be the "hard" setting is now the only thing recording does.
TEST (SeamIsAGapLikeAnyOther, TheJoinIsHeldAsPlayed)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (4, at (0.f));
  pattern.setTick (8, at (4.f));

  closeRecordingSeams (pattern);

  // 9..15 were never written; they hold what tick 8 played rather than
  // travelling back towards tick 4.
  EXPECT_NEAR (pattern.getTick (14).x (), 4.f, 0.001f);
}

// And the reach is what decides whether it is played as a movement or a jump,
// from the same plan the drawn line is cut by.
TEST (SeamIsAGapLikeAnyOther, TheReachDecidesWhetherTheJoinIsDrawnThrough)
{
  auto const ticks = circleWithAGapAtTheLoopPoint (256);
  Pattern pattern;
  writeThrough (pattern, ticks);
  closeRecordingSeams (pattern, index_t{ 200 });

  pattern.setFadeReach (0.f);
  EXPECT_TRUE (pattern.getBridgePlan ().bridges.empty ())
      << "at no reach at all, nothing is drawn through";

  pattern.setFadeReach (1.f);
  EXPECT_FALSE (pattern.getBridgePlan ().bridges.empty ())
      << "at full reach the take has no jumps left";
}

// A middle hole is a finger that lifted on purpose. It is held like the join,
// and it is subject to the same reach -- one rule, not two.
TEST (SeamIsAGapLikeAnyOther, AMiddleHoleFollowsTheSameRule)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, at (0.f));
  pattern.setTick (4, at (1.f));
  pattern.setTick (8, at (2.f));
  closeRecordingSeams (pattern);

  EXPECT_NEAR (pattern.getTick (2).x (), 0.f, 0.001f);
  EXPECT_NEAR (pattern.getTick (6).x (), 1.f, 0.001f);
}
