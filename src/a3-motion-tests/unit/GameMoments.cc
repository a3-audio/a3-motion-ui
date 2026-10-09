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

#include <a3-motion-engine/flight/GameMoments.hh>

using namespace a3;

// When a game fits the music, and where its 1 lands: the one rule the pilots'
// hints, the pilots' own games and the DJ's games share.

namespace
{
constexpr int fourFour = 4;
GameTuning const tuning;

MusicCue
heading (MusicSection now, MusicSection next, long long changeBar, float energy = 0.5f)
{
  MusicCue cue;
  cue.section = now;
  cue.next = next;
  cue.changeBar = changeBar;
  cue.energy = energy;
  return cue;
}

MusicCue
groove (float energy = 0.5f)
{
  MusicCue cue;
  cue.energy = energy;
  return cue;
}
}

TEST (GameMoments, AFakeOutLandsOnTheDropTheMusicNames)
{
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 10);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::FakeOut, cue, 20.5, fourFour, tuning), 40.);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::Formation, cue, 20.5, fourFour, tuning), 40.);
}

TEST (GameMoments, ADropTooCloseForTheFigureIsLeftForTheNextFourBarLine)
{
  // Five beats before the drop: too short for an approach and a veer.
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 10);
  auto const climax = climaxBeatsFor (PilotGame::FakeOut, cue, 35., fourFour, tuning);
  EXPECT_DOUBLE_EQ (climax, 48.);
  EXPECT_GE (climax - 35., minLeadBeats (PilotGame::FakeOut, fourFour, tuning));
}

TEST (GameMoments, WithNothingAheadAGameLandsOnTheNextFourBarLine)
{
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::Formation, groove (), 1., fourFour, tuning), 16.);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::Formation, groove (), 13., fourFour, tuning), 32.);
}

TEST (GameMoments, HideAndSeekReappearsWhereTheBreakdownEnds)
{
  auto const breakdown = heading (MusicSection::Breakdown, MusicSection::Build, 12);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::HideAndSeek, breakdown, 2., fourFour, tuning), 48.);
  // A drop named during a build is not the hider's moment.
  auto const build = heading (MusicSection::Build, MusicSection::Drop, 12);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::HideAndSeek, build, 2., fourFour, tuning), 16.);
}

TEST (GameMoments, CallAndResponseCallsFromTheNextDownbeatABarAway)
{
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::CallAndResponse, groove (), 5.5, fourFour, tuning), 12.);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::CallAndResponse, groove (), 4., fourFour, tuning), 8.);
}

TEST (GameMoments, TheClimaxCountsBarsOfTheRunningMeter)
{
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 10);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::FakeOut, cue, 20., 3, tuning), 30.);
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::Formation, groove (), 1., 3, tuning), 12.);
  EXPECT_DOUBLE_EQ (minLeadBeats (PilotGame::FakeOut, 3, tuning), 6.);
}

TEST (GameMoments, AGameGoesOnABarAfterItsOneAndACallAndResponseForItsCalls)
{
  EXPECT_DOUBLE_EQ (afterClimaxBeats (PilotGame::FakeOut, fourFour, tuning), 4.);
  EXPECT_DOUBLE_EQ (afterClimaxBeats (PilotGame::HideAndSeek, fourFour, tuning), 4.);
  EXPECT_DOUBLE_EQ (afterClimaxBeats (PilotGame::CallAndResponse, fourFour, tuning),
                    2. * tuning.exchanges * fourFour);
}

TEST (GameMoments, AMeterOfNoBeatsLandsNowhere)
{
  EXPECT_DOUBLE_EQ (climaxBeatsFor (PilotGame::FakeOut, groove (), 7., 0, tuning), 7.);
  EXPECT_FALSE (fittingGames (heading (MusicSection::Build, MusicSection::Drop, 4), 0, 0, tuning).any ());
}

TEST (GameMoments, ABuildWithinEightBarsOfItsDropFitsTheFakeOutAndTheFormation)
{
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 12);
  auto const fitting = fittingGames (cue, 6, fourFour, tuning);
  EXPECT_TRUE (fitting.fakeOut);
  EXPECT_TRUE (fitting.formation);
  EXPECT_FALSE (fitting.hideAndSeek);
  EXPECT_FALSE (fitting.callAndResponse);
  EXPECT_FALSE (fittingGames (cue, 2, fourFour, tuning).any ()) << "ten bars out: not yet";
}

TEST (GameMoments, InTheLastBarBeforeTheDropOnlyTheFormationStillFits)
{
  auto const fitting = fittingGames (heading (MusicSection::Build, MusicSection::Drop, 12), 11,
                                     fourFour, tuning);
  EXPECT_FALSE (fitting.fakeOut);
  EXPECT_TRUE (fitting.formation);
}

TEST (GameMoments, ABreakdownFitsHideAndSeekWhileItHasTimeLeft)
{
  auto const cue = heading (MusicSection::Breakdown, MusicSection::Build, 20);
  EXPECT_TRUE (fittingGames (cue, 12, fourFour, tuning).hideAndSeek);
  EXPECT_FALSE (fittingGames (cue, 18, fourFour, tuning).hideAndSeek);

  MusicCue endless;
  endless.section = MusicSection::Breakdown;
  endless.energy = 0.5f;
  EXPECT_TRUE (fittingGames (endless, 3, fourFour, tuning).hideAndSeek);
}

TEST (GameMoments, AGrooveOpensAMomentAtTheStartOfEverySixteenBars)
{
  for (auto const bar : { 16LL, 17LL, 19LL, 32LL })
    EXPECT_TRUE (fittingGames (groove (), bar, fourFour, tuning).callAndResponse) << bar;
  for (auto const bar : { 20LL, 31LL })
    EXPECT_FALSE (fittingGames (groove (), bar, fourFour, tuning).callAndResponse) << bar;

  // No room for the gathering and four calls before a change three bars away.
  auto const soon = heading (MusicSection::Groove, MusicSection::Breakdown, 19);
  EXPECT_FALSE (fittingGames (soon, 16, fourFour, tuning).callAndResponse);
}

TEST (GameMoments, ADropFitsNothing)
{
  auto const cue = heading (MusicSection::Drop, MusicSection::Groove, 40);
  EXPECT_FALSE (fittingGames (cue, 32, fourFour, tuning).any ());
}

TEST (GameMoments, QuietMusicFitsNothing)
{
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 12, 0.01f);
  EXPECT_FALSE (fittingGames (cue, 6, fourFour, tuning).any ());
  EXPECT_FALSE (fittingGames (groove (0.f), 16, fourFour, tuning).any ());
}

TEST (GameMoments, FittingGamesAnswersForEveryGame)
{
  FittingGames fitting;
  fitting.hideAndSeek = true;
  EXPECT_TRUE (fitting.any ());
  EXPECT_TRUE (fitting.has (PilotGame::HideAndSeek));
  for (auto const game : { PilotGame::None, PilotGame::FakeOut, PilotGame::Formation,
                           PilotGame::CallAndResponse })
    EXPECT_FALSE (fitting.has (game));
}
