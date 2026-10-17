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

#include <a3-motion-engine/flight/PilotFamily.hh>

using namespace a3;

// The music asks for a family, the floor picks the figure in it. A press is
// always answered; a pilot only when the moment is its own.

namespace
{
GameTuning const tuning;

MusicCue
cue (MusicSection section, float energy = 0.5f, bool previewed = true, long long start = 0,
     std::optional<MusicSection> next = std::nullopt, long long changeBar = -1)
{
  MusicCue out;
  out.section = section;
  out.energy = energy;
  out.previewed = previewed;
  out.sectionStartBar = start;
  out.next = next;
  out.changeBar = changeBar;
  return out;
}

std::optional<PilotFamily>
press (MusicCue const &music, long long bar = 0)
{
  return pilotFamily (music, bar, Asker::Press, tuning);
}

std::optional<PilotFamily>
pilot (MusicCue const &music, long long bar = 0)
{
  return pilotFamily (music, bar, Asker::Pilot, tuning);
}

FittingGames
nothingFits ()
{
  return {};
}
}

TEST (PilotFamily, AQuietBarAsksAPressForSoftAndAPilotForNothing)
{
  auto const quiet = cue (MusicSection::Drop, 0.f);
  ASSERT_TRUE (press (quiet).has_value ());
  EXPECT_EQ (*press (quiet), PilotFamily::Soft);
  EXPECT_FALSE (pilot (quiet).has_value ());
}

TEST (PilotFamily, ADropAsksAPressForHardInAnyOfItsBars)
{
  for (auto const bar : { 32LL, 33LL, 40LL })
    EXPECT_EQ (*press (cue (MusicSection::Drop, 0.9f, true, 32), bar), PilotFamily::Hard) << bar;
}

TEST (PilotFamily, APilotSlapsOnlyInTheDropsFirstTwoBars)
{
  auto const drop = cue (MusicSection::Drop, 0.9f, true, 32);
  EXPECT_EQ (*pilot (drop, 32), PilotFamily::Hard);
  EXPECT_EQ (*pilot (drop, 33), PilotFamily::Hard);
  EXPECT_FALSE (pilot (drop, 34).has_value ()) << "then the contrast: cruise";
  EXPECT_FALSE (pilot (cue (MusicSection::Drop, 0.9f, true, -1), 32).has_value ())
      << "a drop whose start is not known is not a pilot's";
}

TEST (PilotFamily, ADropTheRoomHeardCountsItsBarsToo)
{
  auto const live = cue (MusicSection::Drop, 0.4f, false, 8);
  EXPECT_EQ (*pilot (live, 9), PilotFamily::Hard);
  EXPECT_FALSE (pilot (live, 10).has_value ());
}

TEST (PilotFamily, ABuildRisesABreakdownIsSoftForBoth)
{
  for (auto const asker : { Asker::Press, Asker::Pilot })
    {
      EXPECT_EQ (*pilotFamily (cue (MusicSection::Build), 3, asker, tuning), PilotFamily::Rising);
      EXPECT_EQ (*pilotFamily (cue (MusicSection::Breakdown), 3, asker, tuning), PilotFamily::Soft);
    }
}

TEST (PilotFamily, APreviewedLoudGrooveIsHardForAPressAndAGrooveForAPilot)
{
  auto const loud = cue (MusicSection::Groove, tuning.hardEnergy);
  EXPECT_EQ (*press (loud), PilotFamily::Hard);
  EXPECT_EQ (*pilot (loud), PilotFamily::Groove);
}

TEST (PilotFamily, APreviewedQuietGrooveIsSoftForBoth)
{
  auto const soft = cue (MusicSection::Groove, tuning.softEnergy * 0.5f);
  EXPECT_EQ (*press (soft), PilotFamily::Soft);
  EXPECT_EQ (*pilot (soft), PilotFamily::Soft);
}

TEST (PilotFamily, TheEnergyThresholdsNeedAPreview)
{
  EXPECT_EQ (*press (cue (MusicSection::Groove, 0.95f, false)), PilotFamily::Groove);
  EXPECT_EQ (*press (cue (MusicSection::Groove, 0.1f, false)), PilotFamily::Groove);
}

TEST (ChooseFigure, HardIsASlap)
{
  juce::Random dice (1);
  EXPECT_EQ (*chooseFigure (PilotFamily::Hard, Asker::Pilot, nothingFits (), false, dice),
             PilotGame::Slap);
}

TEST (ChooseFigure, RisingClimbsWhenTheBuildIsLongElseFakesOutOrForms)
{
  juce::Random dice (1);
  FittingGames climb;
  climb.climb = true;
  climb.fakeOut = true;
  EXPECT_EQ (*chooseFigure (PilotFamily::Rising, Asker::Pilot, climb, false, dice), PilotGame::Climb);
  FittingGames shortBuild;
  shortBuild.fakeOut = true;
  EXPECT_EQ (*chooseFigure (PilotFamily::Rising, Asker::Pilot, shortBuild, false, dice),
             PilotGame::FakeOut);
}

TEST (ChooseFigure, APressIsAlwaysAnsweredAPilotMayLetItPass)
{
  juce::Random dice (1);
  EXPECT_EQ (*chooseFigure (PilotFamily::Rising, Asker::Press, nothingFits (), false, dice),
             PilotGame::Climb);
  EXPECT_FALSE (chooseFigure (PilotFamily::Rising, Asker::Pilot, nothingFits (), false, dice));
  EXPECT_EQ (*chooseFigure (PilotFamily::Groove, Asker::Press, nothingFits (), false, dice),
             PilotGame::CallAndResponse);
  EXPECT_FALSE (chooseFigure (PilotFamily::Groove, Asker::Pilot, nothingFits (), false, dice));
}

TEST (ChooseFigure, EncircleIsNeverAPilots)
{
  auto pressed = 0;
  for (auto index = 0; index < 200; ++index)
    {
      // JUCE's first draw of a small seed is always false (bit 30 of seed * a
      // 35-bit multiplier is 0 below 2^11), so spread the seeds.
      auto const seed = static_cast<juce::int64> (index) * 1000003;
      juce::Random dice (seed);
      EXPECT_EQ (*chooseFigure (PilotFamily::Soft, Asker::Pilot, nothingFits (), true, dice),
                 PilotGame::Graze);
      juce::Random again (seed);
      pressed += *chooseFigure (PilotFamily::Soft, Asker::Press, nothingFits (), true, again)
                         == PilotGame::Encircle
                     ? 1
                     : 0;
    }
  EXPECT_GT (pressed, 0) << "a press may encircle";
}

TEST (ChooseFigure, WithNoRoomToGrazeItIsHideAndSeek)
{
  juce::Random dice (1);
  for (auto const asker : { Asker::Press, Asker::Pilot })
    EXPECT_EQ (*chooseFigure (PilotFamily::Soft, asker, nothingFits (), false, dice),
               PilotGame::HideAndSeek);
}

TEST (ChooseFigure, SameSeedSameFigure)
{
  FittingGames both;
  both.fakeOut = true;
  both.formation = true;
  for (auto seed = 0; seed < 20; ++seed)
    {
      juce::Random a (seed), b (seed);
      EXPECT_EQ (chooseFigure (PilotFamily::Rising, Asker::Pilot, both, false, a),
                 chooseFigure (PilotFamily::Rising, Asker::Pilot, both, false, b));
    }
}

TEST (PlayableFigure, ASoftFigureWithNothingToPlayAgainstHidesInstead)
{
  EXPECT_EQ (playableFigure (PilotGame::Graze, true, false), PilotGame::HideAndSeek);
  EXPECT_EQ (playableFigure (PilotGame::Encircle, false, false), PilotGame::HideAndSeek);
  EXPECT_EQ (playableFigure (PilotGame::Encircle, true, false), PilotGame::Encircle);
  EXPECT_EQ (playableFigure (PilotGame::Slap, false, false), PilotGame::Slap);
}
