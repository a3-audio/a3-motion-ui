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

#include <a3-motion-engine/flight/PilotGames.hh>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace a3;

// The board with the contrast figures on it: a press asks the music (or its
// style) for a figure and lands it on the next beat; the pilots climb,
// slap and graze at FLY.

namespace
{
constexpr int fourFour = 4;
GameTuning const tuning;

std::array<Vec2, flightShips> const spread{ Vec2{ 0.7f, 0.f }, Vec2{ 0.f, 0.7f },
                                            Vec2{ -0.7f, 0.f }, Vec2{ 0.f, -0.7f } };

GameShips
flying (bool orbiting = true)
{
  GameShips ships{};
  for (auto s = 0; s < flightShips; ++s)
    {
      auto const p = spread[static_cast<size_t> (s)];
      ships[static_cast<size_t> (s)]
          = { true, ShipResume{ orbiting, noBodyId }, ShipState{ p, Vec2{ -p.y, p.x } * 0.5f } };
    }
  return ships;
}

GameRequest
asking (PilotGame game, int leader, PilotRecruit with = PilotRecruit::Self)
{
  GameRequest request;
  request.order.game = game;
  request.order.with = with;
  request.leader = leader;
  return request;
}

GameRequest
styled (PilotStyle style, int leader, PilotRecruit with = PilotRecruit::Self)
{
  GameRequest request;
  request.order.style = style;
  request.order.with = with;
  request.leader = leader;
  return request;
}

MusicCue
heading (MusicSection now, std::optional<MusicSection> next, long long changeBar,
         bool previewed = false, long long start = 0, float energy = 0.5f)
{
  MusicCue cue;
  cue.section = now;
  cue.next = next;
  cue.changeBar = changeBar;
  cue.previewed = previewed;
  cue.sectionStartBar = start;
  cue.energy = energy;
  return cue;
}

FlightBodies
oneGroupAt (Vec2 at)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { at, FlightTuning{}.groupMass, 2 };
  return bodies;
}

int
crewOf (PilotGames const &games, PilotGame game)
{
  auto count = 0;
  for (auto s = 0; s < flightShips; ++s)
    if (auto const played = games.gameOf (s); played && played->game == game)
      ++count;
  return count;
}

PilotGame
gameOn (PilotGames const &games, int ship)
{
  auto const played = games.gameOf (ship);
  return played ? played->game : PilotGame::None;
}

/** The leader of the pilots' game, or -1. */
int
pilotLeader (PilotGames const &games)
{
  for (auto s = 0; s < flightShips; ++s)
    if (auto const played = games.gameOf (s); played && played->byPilot)
      return played->leader;
  return -1;
}
}

TEST (PressLanding, AFigureAskedMidBeatOpensOnTheNextBeat)
{
  PilotGames games (1);
  ASSERT_TRUE (games.request (asking (PilotGame::Slap, 0), flying (), {}, {}, 5.3, fourFour));
  EXPECT_TRUE (games.plays (0));
  EXPECT_FALSE (games.steerOf (0, 5.9, fourFour).has_value ()) << "it flies on until its beat";
  EXPECT_FALSE (games.stanceOf (0, 5.9, fourFour).hold);
  EXPECT_TRUE (games.steerOf (0, 6., fourFour).has_value ());
  EXPECT_TRUE (games.stanceOf (0, 6., fourFour).hold);
}

TEST (PressLanding, ASlapAskedForTheBreathsBeatOpensOnTheFollowingOneAtTheFront)
{
  PilotGames games (1);
  games.request (asking (PilotGame::Slap, 0), flying (), {}, {}, 2.5, fourFour);
  EXPECT_FALSE (games.steerOf (0, 3.5, fourFour).has_value ());
  auto const opened = games.steerOf (0, 4., fourFour);
  ASSERT_TRUE (opened.has_value ());
  EXPECT_EQ (opened->at, (Vec2{ tuning.slapSideRadius, 0.f })) << "on a 1: the snap to the front";
}

TEST (PressLanding, OffTheOneASlapOpensOnItsFirstSide)
{
  PilotGames games (1);
  games.request (asking (PilotGame::Slap, 0), flying (), {}, {}, 4.4, fourFour);
  auto const opened = games.steerOf (0, 5., fourFour);
  ASSERT_TRUE (opened.has_value ());
  EXPECT_NE (opened->at, (Vec2{ tuning.slapSideRadius, 0.f })) << "no snap off the 1";
}

TEST (PressLanding, TheFourFirstGamesStartAtOnceAsBefore)
{
  PilotGames games (1);
  games.request (asking (PilotGame::FakeOut, 0), flying (), {}, {}, 5.3, fourFour);
  EXPECT_TRUE (games.steerOf (0, 5.3, fourFour).has_value ());
}

TEST (PressFigure, AutoAsksTheMusicForItsFamily)
{
  auto const figureFor = [] (MusicCue const &cue) {
    PilotGames games (1);
    games.request (styled (PilotStyle::Auto, 0), flying (), {}, cue, 0., fourFour);
    return gameOn (games, 0);
  };
  EXPECT_EQ (figureFor (heading (MusicSection::Drop, MusicSection::Groove, 16)), PilotGame::Slap);
  EXPECT_EQ (figureFor (heading (MusicSection::Build, MusicSection::Drop, 12, true)),
             PilotGame::Climb);
  EXPECT_EQ (figureFor (heading (MusicSection::Breakdown, MusicSection::Build, 8)),
             PilotGame::HideAndSeek)
      << "no group to graze";
  EXPECT_EQ (figureFor (heading (MusicSection::Groove, std::nullopt, -1)),
             PilotGame::CallAndResponse);
}

TEST (PressFigure, AStyleChoosesTheFamilyTheMusicStillTheFigure)
{
  PilotGames hard (1);
  hard.request (styled (PilotStyle::Hard, 0), flying (), {},
                heading (MusicSection::Breakdown, MusicSection::Build, 8), 0., fourFour);
  EXPECT_EQ (gameOn (hard, 0), PilotGame::Slap);
  PilotGames soft (1);
  soft.request (styled (PilotStyle::Soft, 1), flying (), {},
                heading (MusicSection::Drop, MusicSection::Groove, 16), 0., fourFour);
  EXPECT_EQ (gameOn (soft, 1), PilotGame::HideAndSeek) << "soft, and no group to graze";
}

TEST (PressFigure, AGameNamedBeatsTheStyle)
{
  PilotGames games (1);
  auto request = asking (PilotGame::Slap, 0);
  request.order.style = PilotStyle::Soft;
  games.request (request, flying (), {}, {}, 0., fourFour);
  EXPECT_EQ (gameOn (games, 0), PilotGame::Slap);
}

TEST (PressFigure, StyleNoneAsksForNothing)
{
  PilotGames games (1);
  EXPECT_FALSE (games.request (styled (PilotStyle::None, 0), flying (), {}, {}, 0., fourFour));
  EXPECT_FALSE (games.plays (0));
}

TEST (PressFigure, ASoftFigureWithNoGroupIsPlayedAsHideAndSeek)
{
  for (auto const game : { PilotGame::Graze, PilotGame::Encircle })
    {
      PilotGames games (1);
      games.request (asking (game, 0), flying (), {}, {}, 0., fourFour);
      EXPECT_EQ (gameOn (games, 0), PilotGame::HideAndSeek) << pilotWord (game);
    }
}

TEST (PressFigure, ASlapAndAnEncircleTakeAtMostAPairAGrazeOneShip)
{
  auto const crewFor = [] (PilotGame game) {
    PilotGames games (1);
    games.request (asking (game, 0, PilotRecruit::All), flying (),
                   oneGroupAt ({ -0.5f, 0.3f }), {}, 0., fourFour);
    return crewOf (games, game);
  };
  EXPECT_EQ (crewFor (PilotGame::Slap), 2);
  EXPECT_EQ (crewFor (PilotGame::Encircle), 2);
  EXPECT_EQ (crewFor (PilotGame::Graze), 1);
  EXPECT_EQ (crewFor (PilotGame::Climb), 4);
}

TEST (PressFigure, ASecondPressReplacesTheFirstAndNoShipPlaysTwice)
{
  PilotGames games (1);
  games.request (asking (PilotGame::Slap, 0, PilotRecruit::Nearest), flying (), {}, {}, 5.3,
                 fourFour);
  auto partner = -1;
  for (auto s = 1; s < flightShips; ++s)
    if (games.plays (s))
      partner = s;
  ASSERT_GE (partner, 1);
  games.request (asking (PilotGame::Climb, 0), flying (), {}, {}, 5.5, fourFour);
  EXPECT_EQ (gameOn (games, 0), PilotGame::Climb) << "the newer press wins";
  EXPECT_EQ (gameOn (games, partner), PilotGame::Slap) << "the rest play on";
  auto const third = partner == 2 ? 3 : 2;
  games.request (asking (PilotGame::Slap, third, PilotRecruit::Nearest), flying (), {}, {}, 5.6,
                 fourFour);
  EXPECT_EQ (gameOn (games, 0), PilotGame::Climb) << "a busy ship is nobody's partner";
  EXPECT_EQ (games.gameOf (partner)->leader, 0) << "still in the first slap";
  auto const last = 6 - partner - third; // the one ship that was free
  ASSERT_TRUE (games.gameOf (last).has_value ());
  EXPECT_EQ (games.gameOf (last)->leader, third) << "the new slap took the free ship";
}

TEST (PilotGames, AShipReleasedFromAGameLeavesTheRestPlaying)
{
  PilotGames games (1);
  games.request (asking (PilotGame::Formation, 0, PilotRecruit::All), flying (), {}, {}, 0.,
                 fourFour);
  games.release (2);
  EXPECT_FALSE (games.plays (2));
  EXPECT_TRUE (games.plays (0));
  EXPECT_TRUE (games.plays (3));
}

TEST (ClimbChain, AClimbIntoAPreviewedDropBecomesASlapOnTheOne)
{
  PilotGames games (1);
  auto const build = heading (MusicSection::Build, MusicSection::Drop, 12, true, 4);
  ASSERT_TRUE (games.request (asking (PilotGame::Climb, 0), flying (), {}, build, 0., fourFour));
  games.step (flying (), {}, build, PilotLevel::Off, 47.9, fourFour);
  EXPECT_EQ (gameOn (games, 0), PilotGame::Climb);
  auto const drop = heading (MusicSection::Drop, MusicSection::Breakdown, 28, true, 12, 0.9f);
  games.step (flying (), {}, drop, PilotLevel::Off, 48., fourFour);
  EXPECT_EQ (gameOn (games, 0), PilotGame::Slap);
  EXPECT_TRUE (games.stanceOf (0, 48., fourFour).hold);
  EXPECT_EQ (games.steerOf (0, 48., fourFour)->at, (Vec2{ tuning.slapSideRadius, 0.f }))
      << "the snap to the front on the 1";
  games.step (flying (), {}, drop, PilotLevel::Off, 56., fourFour);
  EXPECT_FALSE (games.plays (0)) << "two bars of it, then cruise";
}

TEST (ClimbChain, APreviewAMomentLateStillChains)
{
  PilotGames games (1);
  auto const build = heading (MusicSection::Build, MusicSection::Drop, 12, true, 4);
  games.request (asking (PilotGame::Climb, 0), flying (), {}, build, 0., fourFour);
  games.step (flying (), {}, build, PilotLevel::Off, 48., fourFour);
  EXPECT_EQ (gameOn (games, 0), PilotGame::Slap);
}

TEST (ClimbChain, NoDropOnTheOneMeansNoSlap)
{
  PilotGames games (1);
  auto const build = heading (MusicSection::Build, MusicSection::Drop, 12, true, 4);
  games.request (asking (PilotGame::Climb, 0), flying (), {}, build, 0., fourFour);
  auto const looped = heading (MusicSection::Build, MusicSection::Drop, 16, true, 4);
  games.step (flying (), {}, looped, PilotLevel::Off, 48., fourFour);
  EXPECT_EQ (gameOn (games, 0), PilotGame::Climb) << "it holds at the top";
  games.step (flying (), {}, looped, PilotLevel::Off, 48. + tuning.climbHoldBars * fourFour,
              fourFour);
  EXPECT_FALSE (games.plays (0)) << "then lets go, without a snap";
}

TEST (ClimbChain, AChainedSlapKeepsAPairAtMost)
{
  PilotGames games (1);
  auto const build = heading (MusicSection::Build, MusicSection::Drop, 12, true, 4);
  games.request (asking (PilotGame::Climb, 0, PilotRecruit::All), flying (), {}, build, 0.,
                 fourFour);
  ASSERT_EQ (crewOf (games, PilotGame::Climb), 4);
  games.step (flying (), {}, build, PilotLevel::Off, 48., fourFour);
  EXPECT_EQ (crewOf (games, PilotGame::Slap), 2);
  EXPECT_EQ (crewOf (games, PilotGame::Climb), 0);
}

TEST (SoftFigures, AnEncircleFollowsItsGroup)
{
  PilotGames games (1);
  auto bodies = oneGroupAt ({ -0.4f, 0.3f });
  games.request (asking (PilotGame::Encircle, 0), flying (), bodies, {}, 8., fourFour);
  bodies.body[0].at = { 0.3f, -0.4f };
  games.step (flying (), bodies, {}, PilotLevel::Off, 9., fourFour);
  auto const goal = games.steerOf (0, 9., fourFour);
  ASSERT_TRUE (goal.has_value ());
  EXPECT_LT (goal->at.getDistanceFrom ({ 0.3f, -0.4f }), 0.5f) << "round the group where it is now";
}

TEST (SoftFigures, AGroupRemovedMidFigureLeavesItFinite)
{
  for (auto const game : { PilotGame::Encircle, PilotGame::Graze })
    {
      PilotGames games (1);
      games.request (asking (game, 0), flying (), oneGroupAt ({ -0.5f, 0.3f }), {}, 8., fourFour);
      games.step (flying (), {}, {}, PilotLevel::Off, 9., fourFour);
      auto const goal = games.steerOf (0, 9.5, fourFour);
      ASSERT_TRUE (goal.has_value ()) << pilotWord (game);
      EXPECT_TRUE (std::isfinite (goal->at.x) && std::isfinite (goal->at.y)) << pilotWord (game);
      EXPECT_GT (goal->at.getDistanceFromOrigin (), 0.1f) << "no jump to the middle";
    }
}

TEST (PressFigure, AGrazeWithNoRoomForItsPassIsPlayedAsHideAndSeek)
{
  PilotGames graze (1);
  graze.request (asking (PilotGame::Graze, 0), flying (), oneGroupAt ({ 0.f, 0.f }), {}, 0.,
                 fourFour);
  EXPECT_EQ (gameOn (graze, 0), PilotGame::HideAndSeek);
  PilotGames encircle (1);
  encircle.request (asking (PilotGame::Encircle, 0), flying (), oneGroupAt ({ 0.f, 0.f }), {}, 0.,
                    fourFour);
  EXPECT_EQ (gameOn (encircle, 0), PilotGame::Encircle) << "a circle needs only a group";
}

TEST (PilotsFly, ALongPreviewedBuildIsClimbedAndItsDropSlapped)
{
  PilotGames games (1);
  auto const build = heading (MusicSection::Build, MusicSection::Drop, 12, true, 0, 0.6f);
  games.step (flying (), {}, build, PilotLevel::Fly, 0., fourFour);
  auto const climber = pilotLeader (games);
  ASSERT_GE (climber, 0);
  EXPECT_EQ (gameOn (games, climber), PilotGame::Climb);
  auto const drop = heading (MusicSection::Drop, MusicSection::Breakdown, 28, true, 12, 0.9f);
  games.step (flying (), {}, drop, PilotLevel::Fly, 48., fourFour);
  EXPECT_EQ (gameOn (games, climber), PilotGame::Slap);
  games.step (flying (), {}, drop, PilotLevel::Fly, 56., fourFour);
  EXPECT_LT (pilotLeader (games), 0) << "two bars of slap, then cruise";
}

TEST (PilotsFly, APreviewedDropNobodyClimbedIsSlappedOnItsOne)
{
  PilotGames games (1);
  auto const drop = heading (MusicSection::Drop, MusicSection::Breakdown, 28, true, 12, 0.9f);
  games.step (flying (), {}, drop, PilotLevel::Fly, 48.05, fourFour); // the preview a moment late
  auto const leader = pilotLeader (games);
  ASSERT_GE (leader, 0);
  EXPECT_EQ (gameOn (games, leader), PilotGame::Slap);
  EXPECT_EQ (games.steerOf (leader, 48.05, fourFour)->at, (Vec2{ tuning.slapSideRadius, 0.f }))
      << "on the 1 it was for, with the snap";
}

TEST (PilotsFly, PastTheDropsSecondBarThePilotsLetItCruise)
{
  PilotGames games (1);
  auto const drop = heading (MusicSection::Drop, MusicSection::Breakdown, 28, true, 12, 0.9f);
  games.step (flying (), {}, drop, PilotLevel::Fly, 56., fourFour);
  EXPECT_LT (pilotLeader (games), 0);
}

TEST (PilotsFly, ACueThatChangesMidBarIsSeenAtOnce)
{
  PilotGames games (1);
  auto const groove = heading (MusicSection::Groove, MusicSection::Drop, 12, true, 4, 0.5f);
  games.step (flying (), {}, groove, PilotLevel::Fly, 48., fourFour); // bar 12: outside its window
  ASSERT_LT (pilotLeader (games), 0);
  auto const drop = heading (MusicSection::Drop, MusicSection::Breakdown, 28, true, 12, 0.9f);
  games.step (flying (), {}, drop, PilotLevel::Fly, 48.1, fourFour); // the same bar
  EXPECT_GE (pilotLeader (games), 0);
}

TEST (PilotsFly, ABreakdownIsGrazedWithAGroupAndHiddenWithout)
{
  auto const breakdown = heading (MusicSection::Breakdown, MusicSection::Build, 20, true, 4);
  PilotGames withGroup (1);
  withGroup.step (flying (), oneGroupAt ({ -0.5f, 0.3f }), breakdown, PilotLevel::Fly, 16.,
                  fourFour);
  EXPECT_EQ (gameOn (withGroup, pilotLeader (withGroup)), PilotGame::Graze);
  PilotGames without (1);
  without.step (flying (), {}, breakdown, PilotLevel::Fly, 16., fourFour);
  EXPECT_EQ (gameOn (without, pilotLeader (without)), PilotGame::HideAndSeek);
}

TEST (PilotsFly, APilotNeverEncircles)
{
  auto const breakdown = heading (MusicSection::Breakdown, MusicSection::Build, 20, true, 4);
  for (auto seed = 0; seed < 100; ++seed)
    {
      PilotGames games (seed);
      games.step (flying (), oneGroupAt ({ -0.5f, 0.3f }), breakdown, PilotLevel::Fly, 16.,
                  fourFour);
      EXPECT_NE (gameOn (games, std::max (0, pilotLeader (games))), PilotGame::Encircle) << seed;
    }
}

TEST (PilotsFly, QuietMusicStartsNothing)
{
  PilotGames games (1);
  games.step (flying (), {}, heading (MusicSection::Drop, std::nullopt, -1, true, 12, 0.f),
              PilotLevel::Fly, 48., fourFour);
  EXPECT_LT (pilotLeader (games), 0);
}

TEST (LiveMoodFamilies, ADropTheRoomHeardIsSlappedForABarWithoutTheSnap)
{
  PilotGames games (1);
  auto const heard = heading (MusicSection::Drop, MusicSection::Groove, 16, false, 8, 0.4f);
  games.step (flying (), {}, heard, PilotLevel::Fly, 36., fourFour); // bar 9: the drop was bar 8
  auto const leader = pilotLeader (games);
  ASSERT_GE (leader, 0);
  EXPECT_EQ (gameOn (games, leader), PilotGame::Slap);
  EXPECT_NE (games.steerOf (leader, 36., fourFour)->at, (Vec2{ tuning.slapSideRadius, 0.f }))
      << "its 1 has passed: no snap";
  games.step (flying (), {}, heard, PilotLevel::Fly, 40., fourFour);
  EXPECT_FALSE (games.plays (leader)) << "over two bars after the drop";
}

TEST (LiveMoodFamilies, ABuildTheRoomHeardIsClimbedToItsLineAndLetGoWithoutASnap)
{
  PilotGames games (1);
  auto const heard = heading (MusicSection::Build, MusicSection::Drop, 16, false, 13, 0.5f);
  games.step (flying (), {}, heard, PilotLevel::Fly, 52., fourFour); // bar 13, three to the line
  auto const climber = pilotLeader (games);
  ASSERT_GE (climber, 0);
  EXPECT_EQ (gameOn (games, climber), PilotGame::Climb);
  auto const drop = heading (MusicSection::Drop, MusicSection::Groove, 24, false, 16, 0.6f);
  games.step (flying (), {}, drop, PilotLevel::Fly, 64., fourFour);
  EXPECT_EQ (gameOn (games, climber), PilotGame::Slap) << "the climb yields to the drop it hears";
  auto const opening = games.steerOf (climber, 64., fourFour);
  ASSERT_TRUE (opening.has_value ());
  EXPECT_NE (opening->at, (Vec2{ tuning.slapSideRadius, 0.f })) << "its 1 has passed: no snap";
  games.step (flying (), {}, drop, PilotLevel::Fly, 72., fourFour);
  EXPECT_FALSE (games.plays (climber)) << "over by the end of the drop's second bar";
}
