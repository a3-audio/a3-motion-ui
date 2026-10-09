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

#include <vector>

using namespace a3;

// The board the games are played on: who plays in which game, when a ship
// leaves one, when a game is over, and when a pilot starts one of its own.
// Never two games on one ship; a recruiting game takes only free ships.

namespace
{
constexpr int fourFour = 4;

std::array<Vec2, flightShips> const spread{ Vec2{ 0.7f, 0.f }, Vec2{ 0.f, 0.7f },
                                            Vec2{ -0.7f, 0.f }, Vec2{ 0.f, -0.7f } };

/** Four ships that can fly, each patrolling (or on its clip), counter-clockwise. */
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

MusicCue
heading (MusicSection now, std::optional<MusicSection> next, long long changeBar)
{
  MusicCue cue;
  cue.section = now;
  cue.next = next;
  cue.changeBar = changeBar;
  cue.energy = 0.5f;
  return cue;
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

bool
anyPlays (PilotGames const &games)
{
  for (auto s = 0; s < flightShips; ++s)
    if (games.plays (s))
      return true;
  return false;
}

std::optional<ShipGame>
aPilotsGame (PilotGames const &games)
{
  for (auto s = 0; s < flightShips; ++s)
    if (auto const played = games.gameOf (s); played && played->byPilot)
      return played;
  return std::nullopt;
}
}

TEST (PilotLevel, TheWordsReadBack)
{
  for (auto const level : { PilotLevel::Off, PilotLevel::Hint, PilotLevel::Fly })
    {
      auto const back = pilotLevelNamed (pilotLevelWord (level));
      ASSERT_TRUE (back.has_value ());
      EXPECT_EQ (*back, level);
    }
  EXPECT_EQ (pilotLevelWord (PilotLevel::Hint), "hint");
  EXPECT_FALSE (pilotLevelNamed ("warp").has_value ());
  EXPECT_FALSE (pilotLevelNamed ("").has_value ());
}

TEST (PilotLevel, TheKeyStepsOffHintFlyAndRoundAgain)
{
  EXPECT_EQ (nextPilotLevel (PilotLevel::Off), PilotLevel::Hint);
  EXPECT_EQ (nextPilotLevel (PilotLevel::Hint), PilotLevel::Fly);
  EXPECT_EQ (nextPilotLevel (PilotLevel::Fly), PilotLevel::Off);
}

TEST (PilotGames, ADjGameTakesItsLeaderAndTheShipsItBrings)
{
  PilotGames games (1);
  ASSERT_TRUE (games.request (asking (PilotGame::Formation, 1, PilotRecruit::All), flying (), {},
                              {}, 0., fourFour));
  EXPECT_EQ (crewOf (games, PilotGame::Formation), 4);
  auto const played = games.gameOf (2);
  ASSERT_TRUE (played.has_value ());
  EXPECT_EQ (played->leader, 1);
  EXPECT_FALSE (played->byPilot);
}

TEST (PilotGames, NoShipPlaysTwoGames)
{
  PilotGames games (1);
  games.request (asking (PilotGame::FakeOut, 1), flying (), {}, {}, 0., fourFour);
  games.request (asking (PilotGame::Formation, 0, PilotRecruit::All), flying (), {}, {}, 0.,
                 fourFour);
  EXPECT_EQ (games.gameOf (1)->game, PilotGame::FakeOut);
  EXPECT_EQ (crewOf (games, PilotGame::Formation), 3);
}

TEST (PilotGames, ANewGameTakesItsLeaderOutOfTheOldOneAndTheRestPlayOn)
{
  PilotGames games (1);
  games.request (asking (PilotGame::Formation, 0, PilotRecruit::All), flying (), {}, {}, 0.,
                 fourFour);
  games.request (asking (PilotGame::HideAndSeek, 2), flying (), {}, {}, 0., fourFour);
  EXPECT_EQ (games.gameOf (2)->game, PilotGame::HideAndSeek);
  EXPECT_EQ (crewOf (games, PilotGame::Formation), 3);
}

TEST (PilotGames, CallAndResponseIsAlwaysAPair)
{
  for (auto const with : { PilotRecruit::Self, PilotRecruit::Nearest, PilotRecruit::All })
    {
      PilotGames games (1);
      games.request (asking (PilotGame::CallAndResponse, 0, with), flying (), {}, {}, 0., fourFour);
      EXPECT_EQ (crewOf (games, PilotGame::CallAndResponse), 2) << pilotWord (with);
    }
}

TEST (PilotGames, CallAndResponseIsAloneWhenNoShipIsFree)
{
  PilotGames games (1);
  for (auto const busy : { 1, 2, 3 })
    games.request (asking (PilotGame::FakeOut, busy), flying (), {}, {}, 0., fourFour);
  games.request (asking (PilotGame::CallAndResponse, 0, PilotRecruit::All), flying (), {}, {},
                 0., fourFour);
  EXPECT_EQ (crewOf (games, PilotGame::CallAndResponse), 1);
  EXPECT_EQ (crewOf (games, PilotGame::FakeOut), 3);
}

TEST (PilotGames, ALeaderThatCannotFlyStartsNothing)
{
  PilotGames games (1);
  auto ships = flying ();
  ships[0].canFly = false;
  EXPECT_FALSE (games.request (asking (PilotGame::FakeOut, 0), ships, {}, {}, 0., fourFour));
  EXPECT_FALSE (anyPlays (games));
  EXPECT_FALSE (games.request (asking (PilotGame::FakeOut, 7), flying (), {}, {}, 0., fourFour));
  EXPECT_FALSE (games.request (asking (PilotGame::None, 1), flying (), {}, {}, 0., fourFour));
  GameRequest nothing;
  nothing.leader = 1;
  EXPECT_FALSE (games.request (nothing, flying (), {}, {}, 0., fourFour));
  EXPECT_FALSE (anyPlays (games));
}

TEST (PilotGames, AShipThatCannotFlyIsNotRecruited)
{
  PilotGames games (1);
  auto ships = flying ();
  ships[2].canFly = false;
  games.request (asking (PilotGame::Formation, 0, PilotRecruit::All), ships, {}, {}, 0., fourFour);
  EXPECT_EQ (crewOf (games, PilotGame::Formation), 3);
  EXPECT_FALSE (games.plays (2));
}

TEST (PilotGames, AShipTheDjSendsElsewhereLeavesItsGameAtOnce)
{
  PilotGames games (1);
  auto ships = flying ();
  games.request (asking (PilotGame::Formation, 0, PilotRecruit::All), ships, {}, {}, 0., fourFour);
  ships[3].now.bodyId = 4; // the DJ sent it to escort a group
  games.step (ships, {}, {}, PilotLevel::Off, 0.1, fourFour);
  EXPECT_FALSE (games.plays (3));
  EXPECT_EQ (crewOf (games, PilotGame::Formation), 3);
  ships[1].now.orbiting = false; // and this one back to its clip
  games.step (ships, {}, {}, PilotLevel::Off, 0.2, fourFour);
  EXPECT_FALSE (games.plays (1));
  EXPECT_EQ (crewOf (games, PilotGame::Formation), 2);
}

TEST (PilotGames, AShipBorrowedFromItsClipStaysWhileTheDjLeavesItThere)
{
  PilotGames games (1);
  auto const onClips = flying (false);
  games.request (asking (PilotGame::Formation, 0, PilotRecruit::All), onClips, {}, {}, 0., fourFour);
  games.step (onClips, {}, {}, PilotLevel::Off, 0.1, fourFour);
  EXPECT_EQ (crewOf (games, PilotGame::Formation), 4);
}

TEST (PilotGames, AShipThatStopsFlyingLeavesAndTheLastOneEndsTheGame)
{
  PilotGames games (1);
  auto ships = flying ();
  games.request (asking (PilotGame::FakeOut, 0), ships, {}, {}, 0., fourFour);
  ships[0].canFly = false;
  games.step (ships, {}, {}, PilotLevel::Off, 0.1, fourFour);
  EXPECT_FALSE (games.gameOf (0).has_value ());
  ships[0].canFly = true;
  games.step (ships, {}, {}, PilotLevel::Off, 0.2, fourFour);
  EXPECT_FALSE (games.plays (0)) << "the game ended with its last ship";
}

TEST (PilotGames, AGameEndsAfterItsLastBar)
{
  PilotGames games (1);
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4); // its 1 on beat 16
  games.request (asking (PilotGame::FakeOut, 0), flying (), {}, cue, 0., fourFour);
  games.step (flying (), {}, cue, PilotLevel::Off, 19.9, fourFour);
  EXPECT_TRUE (games.plays (0));
  games.step (flying (), {}, cue, PilotLevel::Off, 20., fourFour);
  EXPECT_FALSE (games.plays (0));
}

TEST (PilotGames, EndingAGameOnAnyOfItsShipsEndsItForAll)
{
  PilotGames games (1);
  games.request (asking (PilotGame::Formation, 0, PilotRecruit::All), flying (), {}, {}, 0.,
                 fourFour);
  games.endGameOn (2);
  EXPECT_FALSE (anyPlays (games));
  games.request (asking (PilotGame::FakeOut, 1), flying (), {}, {}, 0., fourFour);
  games.endAll ();
  EXPECT_FALSE (anyPlays (games));
}

TEST (PilotGames, OnlyItsCrewIsSteered)
{
  PilotGames games (1);
  games.request (asking (PilotGame::FakeOut, 1), flying (), {}, {}, 0., fourFour);
  EXPECT_TRUE (games.steerOf (1, 1., fourFour).has_value ());
  EXPECT_FALSE (games.steerOf (0, 1., fourFour).has_value ());
  EXPECT_FALSE (games.steerOf (9, 1., fourFour).has_value ());
  EXPECT_FALSE (games.gameOf (-1).has_value ());
}

TEST (PilotGames, TheTargetIsFollowedWhenTheGroupIsMoved)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { { -0.5f, 0.3f }, FlightTuning{}.groupMass, 0 };
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 4);
  PilotGames games (1);
  games.request (asking (PilotGame::FakeOut, 0), flying (), bodies, cue, 0., fourFour);
  bodies.body[0].at = { 0.3f, -0.5f };
  games.step (flying (), bodies, cue, PilotLevel::Off, 17., fourFour);
  EXPECT_EQ (games.steerOf (0, 17., fourFour)->at, (Vec2{ 0.3f, -0.5f }));
}

TEST (PilotGames, AtFlyAPilotStartsAGameWhenItsMomentOpens)
{
  PilotGames games (1);
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 12);
  games.step (flying (), {}, cue, PilotLevel::Fly, 0., fourFour); // twelve bars out
  EXPECT_FALSE (anyPlays (games));
  games.step (flying (), {}, cue, PilotLevel::Fly, 12., fourFour); // nine bars out
  EXPECT_FALSE (anyPlays (games));
  games.step (flying (), {}, cue, PilotLevel::Fly, 16., fourFour); // eight bars out
  auto const started = aPilotsGame (games);
  ASSERT_TRUE (started.has_value ());
  EXPECT_TRUE (started->game == PilotGame::FakeOut || started->game == PilotGame::Formation);
}

TEST (PilotGames, BelowFlyThePilotsStartNothing)
{
  for (auto const level : { PilotLevel::Off, PilotLevel::Hint })
    {
      PilotGames games (1);
      games.step (flying (), {}, heading (MusicSection::Build, MusicSection::Drop, 12), level,
                  16., fourFour);
      EXPECT_FALSE (anyPlays (games)) << pilotLevelWord (level);
    }
}

TEST (PilotGames, PilotsTakeOnlyShipsOnTheirOrbit)
{
  PilotGames games (1);
  games.step (flying (false), {}, heading (MusicSection::Build, MusicSection::Drop, 12),
              PilotLevel::Fly, 16., fourFour);
  EXPECT_FALSE (anyPlays (games));
}

TEST (PilotGames, OnePilotGameAtATime)
{
  PilotGames games (1);
  auto const cue = heading (MusicSection::Breakdown, std::nullopt, -1);
  games.step (flying (), {}, cue, PilotLevel::Fly, 0., fourFour);
  auto const first = aPilotsGame (games);
  ASSERT_TRUE (first.has_value ());
  games.step (flying (), {}, cue, PilotLevel::Fly, 4., fourFour); // a new bar, still a moment
  for (auto s = 0; s < flightShips; ++s)
    if (auto const played = games.gameOf (s); played && played->byPilot)
      {
        EXPECT_EQ (played->leader, first->leader) << s;
      }
}

TEST (PilotGames, LeavingFlyEndsThePilotsGamesButNotTheDjs)
{
  PilotGames games (1);
  auto const ships = flying ();
  auto const cue = heading (MusicSection::Build, MusicSection::Drop, 12);
  games.request (asking (PilotGame::HideAndSeek, 3), ships, {}, {}, 0., fourFour);
  games.step (ships, {}, cue, PilotLevel::Fly, 16., fourFour);
  ASSERT_TRUE (aPilotsGame (games).has_value ());
  games.step (ships, {}, cue, PilotLevel::Hint, 16.1, fourFour);
  EXPECT_FALSE (aPilotsGame (games).has_value ());
  ASSERT_TRUE (games.gameOf (3).has_value ());
  EXPECT_FALSE (games.gameOf (3)->byPilot);
}

TEST (PilotGames, AfterAGameOfTheirOwnThePilotsRest)
{
  PilotGames games (1);
  auto const ships = flying ();
  auto const cue = heading (MusicSection::Breakdown, std::nullopt, -1);
  games.step (ships, {}, cue, PilotLevel::Fly, 0., fourFour); // hide & seek, its 1 on beat 16
  ASSERT_TRUE (anyPlays (games));
  games.step (ships, {}, cue, PilotLevel::Fly, 20., fourFour);
  EXPECT_FALSE (anyPlays (games)) << "its last bar is over";
  games.step (ships, {}, cue, PilotLevel::Fly, 24., fourFour);
  EXPECT_FALSE (anyPlays (games)) << "resting";
  games.step (ships, {}, cue, PilotLevel::Fly, 32., fourFour);
  EXPECT_FALSE (anyPlays (games)) << "the fourth bar of rest";
  games.step (ships, {}, cue, PilotLevel::Fly, 36., fourFour);
  EXPECT_TRUE (anyPlays (games)) << "rested";
}

TEST (PilotGames, SameSeedSamePilots)
{
  auto const play = [] {
    PilotGames games (11);
    auto const cue = heading (MusicSection::Breakdown, std::nullopt, -1);
    std::vector<float> trace;
    for (auto tick = 0; tick < 4000; ++tick)
      {
        auto const beats = tick * 0.08;
        games.step (flying (), {}, cue, PilotLevel::Fly, beats, fourFour);
        for (auto s = 0; s < flightShips; ++s)
          if (auto const goal = games.steerOf (s, beats, fourFour))
            {
              trace.push_back (goal->at.x);
              trace.push_back (goal->at.y);
            }
          else
            trace.push_back (-9.f);
      }
    return trace;
  };
  EXPECT_EQ (play (), play ());
}
