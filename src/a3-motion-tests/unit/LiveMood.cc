/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/preview/LiveMood.hh>

#include <cmath>
#include <limits>
#include <random>

using namespace a3;

// Without StemDeck the pilots read the room's music from the channel meters:
// a bar's level against the bars before it says breakdown, build or drop,
// later and coarser than a preview, but it works.

namespace
{
/** `count` bars at `level` from bar `from` on; the bar after them. */
long long
play (LiveMood &mood, long long from, int count, float level)
{
  for (auto i = 0; i < count; ++i)
    mood.addBar (from + i, level);
  return from + count;
}
}

TEST (BarMeter, TheLoudestChannelsMeanIsTheBar)
{
  BarMeter meter;
  meter.hear (0, 0.2f);
  meter.hear (0, 0.4f);
  meter.hear (2, 0.1f);
  EXPECT_FLOAT_EQ (meter.closeBar (), 0.3f);
  EXPECT_FLOAT_EQ (meter.closeBar (), 0.f) << "a closed bar starts the next one empty";
}

TEST (BarMeter, OnlyTheFourChannelsAndRealLevelsAreHeard)
{
  BarMeter meter;
  meter.hear (4, 0.9f);
  meter.hear (-1, 0.9f);
  meter.hear (1, std::numeric_limits<float>::quiet_NaN ());
  meter.hear (1, -0.5f);
  meter.hear (1, 0.25f);
  EXPECT_FLOAT_EQ (meter.closeBar (), 0.25f);
}

TEST (LiveMood, ItStartsInAGrooveWithNoChangeAhead)
{
  LiveMood const mood;
  auto const cue = mood.cue ();
  EXPECT_EQ (cue.section, MusicSection::Groove);
  EXPECT_FALSE (cue.next.has_value ());
  EXPECT_EQ (cue.changeBar, -1);
  EXPECT_FALSE (cue.previewed);
}

TEST (LiveMood, SteadyBarsStayAGroove)
{
  LiveMood mood;
  play (mood, 0, 12, 0.5f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
  EXPECT_FLOAT_EQ (mood.cue ().energy, 0.5f);
}

TEST (LiveMood, HalfAsLoudIsABreakdownThatExpectsABuildOnTheNextEightBarLine)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.5f);
  mood.addBar (bar, 0.2f); // bar 8; bar 9 runs
  auto const cue = mood.cue ();
  EXPECT_EQ (cue.section, MusicSection::Breakdown);
  ASSERT_TRUE (cue.next.has_value ());
  EXPECT_EQ (*cue.next, MusicSection::Build);
  EXPECT_EQ (cue.changeBar, 16);
}

TEST (LiveMood, ThreeRisingBarsAfterABreakdownAreABuildThatExpectsADrop)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.5f);
  bar = play (mood, bar, 4, 0.2f); // the breakdown, bars 8..11
  for (auto const level : { 0.24f, 0.28f, 0.32f })
    mood.addBar (bar++, level); // bars 12..14; bar 15 runs
  auto const cue = mood.cue ();
  EXPECT_EQ (cue.section, MusicSection::Build);
  ASSERT_TRUE (cue.next.has_value ());
  EXPECT_EQ (*cue.next, MusicSection::Drop);
  EXPECT_EQ (cue.changeBar, 16);
}

TEST (LiveMood, AJumpIsTheDropAndItIsAGrooveAgainEightBarsLater)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.2f);
  mood.addBar (bar++, 0.6f); // bar 8: three times the bars before
  auto const cue = mood.cue ();
  EXPECT_EQ (cue.section, MusicSection::Drop);
  ASSERT_TRUE (cue.next.has_value ());
  EXPECT_EQ (*cue.next, MusicSection::Groove);
  EXPECT_EQ (cue.changeBar, 16);

  bar = play (mood, bar, 6, 0.6f); // bars 9..14
  EXPECT_EQ (mood.cue ().section, MusicSection::Drop);
  mood.addBar (bar, 0.6f); // bar 15: the drop's eighth bar
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
}

TEST (LiveMood, MusicComingInAfterSilenceIsTheDrop)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove) << "silence says nothing";
  mood.addBar (bar, 0.5f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Drop);
}

TEST (LiveMood, AGapInTheBarsStartsAfresh)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.5f);
  mood.addBar (bar, 0.2f);
  ASSERT_EQ (mood.cue ().section, MusicSection::Breakdown);
  mood.addBar (bar + 5, 0.2f); // the clock jumped
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
  EXPECT_EQ (mood.cue ().changeBar, -1);
}

TEST (LiveMood, ANonFiniteOrNegativeLevelIsNotHeard)
{
  LiveMood mood;
  play (mood, 0, 8, 0.5f);
  mood.addBar (8, std::numeric_limits<float>::quiet_NaN ());
  mood.addBar (8, -1.f);
  mood.addBar (8, std::numeric_limits<float>::infinity ());
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
  EXPECT_FLOAT_EQ (mood.cue ().energy, 0.5f);
  mood.addBar (8, 0.5f); // bar 8 is still the next one
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
}

TEST (LiveMood, TheSameBarsSayTheSameMood)
{
  LiveMood first, second;
  for (auto bar = 0; bar < 40; ++bar)
    {
      auto const level = 0.3f + 0.2f * static_cast<float> ((bar * 7) % 5) / 4.f;
      first.addBar (bar, level);
      second.addBar (bar, level);
      ASSERT_EQ (first.cue ().section, second.cue ().section) << bar;
      ASSERT_EQ (first.cue ().changeBar, second.cue ().changeBar) << bar;
    }
}

// The edges of each threshold, with levels that are exact in binary so that
// "exactly 1.5 times" is exactly that.

TEST (LiveMood, ExactlyOneAndAHalfTimesTheBarsBeforeIsTheDrop)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.25f);
  mood.addBar (bar, 0.375f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Drop);
}

TEST (LiveMood, JustUnderOneAndAHalfTimesIsNoDrop)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.25f);
  mood.addBar (bar, 0.374f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
}

TEST (LiveMood, ExactlyHalfTheBarsBeforeIsTheBreakdown)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.5f);
  mood.addBar (bar, 0.25f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Breakdown);
}

TEST (LiveMood, JustOverHalfTheBarsBeforeIsNoBreakdown)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.5f);
  mood.addBar (bar, 0.26f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
}

TEST (LiveMood, TwoRisingBarsAreNoBuild)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.5f);
  bar = play (mood, bar, 4, 0.2f);
  for (auto const level : { 0.24f, 0.28f })
    mood.addBar (bar++, level);
  EXPECT_EQ (mood.cue ().section, MusicSection::Breakdown);
}

TEST (LiveMood, ARiseThatFallsBackBreaksTheBuild)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.5f);
  bar = play (mood, bar, 4, 0.2f);
  for (auto const level : { 0.24f, 0.28f, 0.2f, 0.24f })
    mood.addBar (bar++, level);
  EXPECT_NE (mood.cue ().section, MusicSection::Build) << "three in a row, not three of four";
}

TEST (LiveMood, BarsThatRiseByLessThanTheStepAreNoBuild)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.5f);
  bar = play (mood, bar, 4, 0.2f);
  for (auto const level : { 0.201f, 0.202f, 0.203f })
    mood.addBar (bar++, level);
  EXPECT_EQ (mood.cue ().section, MusicSection::Breakdown);
}

TEST (LiveMood, TheQuietLevelItselfIsHeardAsMusicComingIn)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.f);
  mood.addBar (bar, MoodTuning{}.quiet);
  EXPECT_EQ (mood.cue ().section, MusicSection::Drop);
}

TEST (LiveMood, JustUnderTheQuietLevelStaysSilent)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.f);
  mood.addBar (bar, MoodTuning{}.quiet - 0.001f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
}

TEST (LiveMood, MusicFadingToSilenceIsABreakdownNotASilentGroove)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 8, 0.5f);
  mood.addBar (bar, 0.f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Breakdown);
}

TEST (LiveMood, TheFirstBarsOfAFreshMoodSayNothingYet)
{
  LiveMood mood;
  mood.addBar (0, 0.1f);
  mood.addBar (1, 0.1f);
  mood.addBar (2, 0.1f);
  mood.addBar (3, 0.9f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove) << "four bars of history first";
}

TEST (LiveMood, TheFifthBarIsTheFirstToSayAnything)
{
  LiveMood fourth;
  play (fourth, 0, 3, 0.1f);
  fourth.addBar (3, 0.9f);
  EXPECT_EQ (fourth.cue ().section, MusicSection::Groove);

  LiveMood fifth;
  play (fifth, 0, 4, 0.1f);
  fifth.addBar (4, 0.9f);
  EXPECT_EQ (fifth.cue ().section, MusicSection::Drop);
}

// Levels exact in binary, so the step is hit exactly.
TEST (LiveMood, ARiseOfExactlyTheStepEntersTheBuildAndJustUnderDoesNot)
{
  MoodTuning tuning;
  tuning.buildStep = 1.5f;
  tuning.dropRise = 100.f; // out of the way
  for (auto const last : { 0.84375f, 0.84f })
    {
      LiveMood mood (tuning);
      auto bar = play (mood, 0, 8, 0.25f);
      for (auto const level : { 0.375f, 0.5625f, last })
        mood.addBar (bar++, level);
      EXPECT_EQ (mood.cue ().section, last == 0.84375f ? MusicSection::Build : MusicSection::Groove)
          << last;
    }
}

TEST (LiveMood, ASwellThatPlateausIsAGrooveAfterOnePhrase)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.5f);
  bar = play (mood, bar, 4, 0.2f);
  for (auto const level : { 0.24f, 0.28f, 0.32f })
    mood.addBar (bar++, level); // the build starts at bar 14
  ASSERT_EQ (mood.cue ().section, MusicSection::Build);
  bar = play (mood, bar, 6, 0.32f); // bars 15..20: still the phrase
  EXPECT_EQ (mood.cue ().section, MusicSection::Build);
  mood.addBar (bar, 0.32f); // bar 21: the eighth bar of the build
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
}

TEST (LiveMood, ATrackThatJustPlaysQuieterIsAGrooveAfterOnePhrase)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.5f);
  mood.addBar (bar++, 0.2f); // bar 8: the breakdown
  ASSERT_EQ (mood.cue ().section, MusicSection::Breakdown);
  bar = play (mood, bar, 6, 0.2f); // bars 9..14
  EXPECT_EQ (mood.cue ().section, MusicSection::Breakdown);
  mood.addBar (bar, 0.2f); // bar 15: the eighth bar of the breakdown
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove);
  play (mood, bar + 1, 8, 0.2f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Groove) << "and it stays";
}

TEST (LiveMood, ABuildThatBecomesADropInsideThePhraseIsTheDrop)
{
  LiveMood mood;
  auto bar = play (mood, 0, 8, 0.5f);
  bar = play (mood, bar, 4, 0.2f);
  for (auto const level : { 0.24f, 0.28f, 0.32f })
    mood.addBar (bar++, level);
  ASSERT_EQ (mood.cue ().section, MusicSection::Build);
  mood.addBar (bar, 0.9f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Drop);
}

TEST (LiveMood, AJitteryButSteadySignalIsNeverABuild)
{
  // +-0.5 dB of wobble around a steady level, 64 bars, a fixed seed.
  std::mt19937 dice (20261009);
  LiveMood mood;
  for (auto bar = 0; bar < 64; ++bar)
    {
      auto const unit = static_cast<float> (dice () % 10001) / 10000.f; // 0..1
      auto const dB = (unit - 0.5f) * 1.f;
      mood.addBar (bar, 0.3f * std::pow (10.f, dB / 20.f));
      ASSERT_NE (mood.cue ().section, MusicSection::Build) << bar;
      ASSERT_NE (mood.cue ().section, MusicSection::Drop) << bar;
      ASSERT_NE (mood.cue ().section, MusicSection::Breakdown) << bar;
    }
}

TEST (LiveMood, TheChangeLineIsTheNextOneStrictlyAfterTheRunningBar)
{
  LiveMood mood;
  auto const bar = play (mood, 0, 15, 0.5f);
  mood.addBar (bar, 0.2f); // bar 15 is over; bar 16 runs, on a line
  ASSERT_EQ (mood.cue ().section, MusicSection::Breakdown);
  EXPECT_EQ (mood.cue ().changeBar, 24);
}

TEST (LiveMood, AHistoryLongerThanWhatIsKeptStillWorks)
{
  MoodTuning tuning;
  tuning.historyBars = 50;
  LiveMood mood (tuning);
  auto const bar = play (mood, 0, 12, 0.2f);
  mood.addBar (bar, 0.9f);
  EXPECT_EQ (mood.cue ().section, MusicSection::Drop);
}
