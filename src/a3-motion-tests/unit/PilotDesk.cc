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

#include <a3-motion-engine/flight/PilotDesk.hh>

using namespace a3;

// Phase B records what an action asks a pilot to play; phase C takes it from
// here. One pending request per ship, the newest wins -- as the newest press
// wins on a clip.

namespace
{
GameRequest
asking (PilotGame game, int leader, PilotRecruit with = PilotRecruit::Self)
{
  GameRequest request;
  request.order.game = game;
  request.order.with = with;
  request.leader = leader;
  return request;
}

std::array<Vec2, flightShips> const spread{ Vec2{ 0.5f, 0.f }, Vec2{ 0.4f, 0.1f },
                                            Vec2{ -0.5f, 0.f }, Vec2{ 0.f, 0.6f } };
std::array<bool, flightShips> const allFree{ true, true, true, true };
std::array<ShipResume, flightShips> const doing{ ShipResume{ true, noBodyId }, ShipResume{ false, noBodyId },
                                                 ShipResume{ true, 3 }, ShipResume{ true, noBodyId } };
}

TEST (PilotDesk, NothingPendsAtFirst)
{
  PilotDesk desk;
  for (auto ship = 0; ship < flightShips; ++ship)
    EXPECT_FALSE (desk.pending (ship).has_value ());
}

TEST (PilotDesk, APostedGameWaitsOnItsShip)
{
  PilotDesk desk;
  desk.post (asking (PilotGame::FakeOut, 2));
  ASSERT_TRUE (desk.pending (2).has_value ());
  EXPECT_EQ (*desk.pending (2)->order.game, PilotGame::FakeOut);
  EXPECT_EQ (desk.pending (2)->leader, 2);
  EXPECT_FALSE (desk.pending (0).has_value ());
}

TEST (PilotDesk, TheNewestRequestWins)
{
  PilotDesk desk;
  desk.post (asking (PilotGame::FakeOut, 1));
  auto const first = desk.pending (1)->serial;
  desk.post (asking (PilotGame::HideAndSeek, 1));
  EXPECT_EQ (*desk.pending (1)->order.game, PilotGame::HideAndSeek);
  EXPECT_GT (desk.pending (1)->serial, first) << "a reader can tell a new request from the old";
}

TEST (PilotDesk, NoneCallsAPendingGameOff)
{
  PilotDesk desk;
  desk.post (asking (PilotGame::Formation, 0));
  desk.post (asking (PilotGame::None, 0));
  EXPECT_FALSE (desk.pending (0).has_value ());
}

TEST (PilotDesk, ARequestWithoutAGameIsNotARequest)
{
  PilotDesk desk;
  desk.post (asking (PilotGame::Formation, 0));
  GameRequest silent;
  silent.leader = 0;
  desk.post (silent);
  ASSERT_TRUE (desk.pending (0).has_value ());
  EXPECT_EQ (*desk.pending (0)->order.game, PilotGame::Formation);
}

TEST (PilotDesk, TakeHandsItOverOnce)
{
  PilotDesk desk;
  desk.post (asking (PilotGame::CallAndResponse, 3));
  auto const taken = desk.take (3);
  ASSERT_TRUE (taken.has_value ());
  EXPECT_EQ (*taken->order.game, PilotGame::CallAndResponse);
  EXPECT_FALSE (desk.take (3).has_value ());
  EXPECT_FALSE (desk.pending (3).has_value ());
}

// Review focus 5.
TEST (PilotDesk, AShipOutsideTheFourIsIgnored)
{
  PilotDesk desk;
  desk.post (asking (PilotGame::FakeOut, flightShips));
  desk.post (asking (PilotGame::FakeOut, -1));
  EXPECT_FALSE (desk.pending (flightShips).has_value ());
  EXPECT_FALSE (desk.take (-1).has_value ());
  for (auto ship = 0; ship < flightShips; ++ship)
    EXPECT_FALSE (desk.pending (ship).has_value ());
}

TEST (PilotDesk, SelfIsTheLeaderAlone)
{
  auto const r = recruit (PilotRecruit::Self, 2, spread, allFree, doing);
  EXPECT_EQ (r.ships, (std::array<bool, flightShips>{ false, false, true, false }));
  EXPECT_TRUE (r.resume[2].orbiting);
  EXPECT_EQ (r.resume[2].bodyId, 3) << "it goes back to escorting G4";
}

TEST (PilotDesk, NearestBringsTheClosestFreeShip)
{
  auto const r = recruit (PilotRecruit::Nearest, 0, spread, allFree, doing);
  EXPECT_EQ (r.ships, (std::array<bool, flightShips>{ true, true, false, false }));
  EXPECT_FALSE (r.resume[1].orbiting) << "ship 1 was flying its clip";

  auto busy = allFree;
  busy[1] = false;
  auto const skipping = recruit (PilotRecruit::Nearest, 0, spread, busy, doing);
  EXPECT_EQ (skipping.ships, (std::array<bool, flightShips>{ true, false, false, true }))
      << "ship 1 is in a game; ship 3 (0.78 away) beats ship 2 (1.0 away)";
}

TEST (PilotDesk, NearestTakesTheLowerChannelOnATie)
{
  std::array<Vec2, flightShips> const tie{ Vec2{ 0.f, 0.f }, Vec2{ 0.f, 0.5f },
                                           Vec2{ 0.5f, 0.f }, Vec2{ 0.9f, 0.f } };
  EXPECT_EQ (recruit (PilotRecruit::Nearest, 0, tie, allFree, doing).ships,
             (std::array<bool, flightShips>{ true, true, false, false }));
}

TEST (PilotDesk, NearestWithNobodyFreeIsTheLeaderAlone)
{
  std::array<bool, flightShips> const none{ false, false, false, false };
  EXPECT_EQ (recruit (PilotRecruit::Nearest, 1, spread, none, doing).ships,
             (std::array<bool, flightShips>{ false, true, false, false }));
}

TEST (PilotDesk, AllBringsEveryFreeShipAndRemembersEach)
{
  auto busy = allFree;
  busy[3] = false;
  auto const r = recruit (PilotRecruit::All, 1, spread, busy, doing);
  EXPECT_EQ (r.ships, (std::array<bool, flightShips>{ true, true, true, false }));
  EXPECT_TRUE (r.resume[0].orbiting);
  EXPECT_EQ (r.resume[2].bodyId, 3);
  EXPECT_FALSE (r.resume[3].orbiting) << "a ship not taken keeps no resume";
}
