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

#include <a3-motion-engine/PilotOrder.hh>

using namespace a3;

// The Pilot section's words (FPV, 2026-10-08): each one read back to
// what it was written from, so the template, the reader and the games
// agree on one spelling.

TEST (PilotOrder, EveryGameIsReadBackFromItsWord)
{
  for (auto const game : { PilotGame::None, PilotGame::FakeOut, PilotGame::Formation,
                           PilotGame::HideAndSeek, PilotGame::CallAndResponse })
    {
      auto const back = pilotGameNamed (pilotWord (game));
      ASSERT_TRUE (back.has_value ()) << pilotWord (game);
      EXPECT_EQ (*back, game);
    }
}

TEST (PilotOrder, TheGamesAreSpelledAsAScriptWritesThem)
{
  EXPECT_EQ (pilotWord (PilotGame::None), "none");
  EXPECT_EQ (pilotWord (PilotGame::FakeOut), "fakeout");
  EXPECT_EQ (pilotWord (PilotGame::Formation), "formation");
  EXPECT_EQ (pilotWord (PilotGame::HideAndSeek), "hideseek");
  EXPECT_EQ (pilotWord (PilotGame::CallAndResponse), "callresponse");
}

TEST (PilotOrder, AGroupIsTheFloorsLabel)
{
  auto const third = pilotTargetNamed ("group3");
  ASSERT_TRUE (third.has_value ());
  EXPECT_EQ (third->kind, PilotTargetKind::Group);
  EXPECT_EQ (third->groupId, 2) << "G3 on the floor is body id 2";
  EXPECT_EQ (pilotWord (*third), "group3");

  for (auto id = 0; id < pilotGroups; ++id)
    {
      auto const back = pilotTargetNamed (pilotWord (PilotTarget{ PilotTargetKind::Group, id }));
      ASSERT_TRUE (back.has_value ()) << id;
      EXPECT_EQ (back->groupId, id);
    }
}

TEST (PilotOrder, OnlyTheEightGroupsAreGroups)
{
  for (auto const *word : { "group0", "group9", "group01", "group", "groupx" })
    EXPECT_FALSE (pilotTargetNamed (word).has_value ()) << word;
}

TEST (PilotOrder, TheOtherTargetsAndTheRecruitsReadBack)
{
  for (auto const kind : { PilotTargetKind::Nearest, PilotTargetKind::Crowd, PilotTargetKind::Hotspot })
    {
      auto const back = pilotTargetNamed (pilotWord (PilotTarget{ kind, -1 }));
      ASSERT_TRUE (back.has_value ());
      EXPECT_EQ (back->kind, kind);
    }
  for (auto const with : { PilotRecruit::Self, PilotRecruit::Nearest, PilotRecruit::All })
    {
      auto const back = pilotRecruitNamed (pilotWord (with));
      ASSERT_TRUE (back.has_value ());
      EXPECT_EQ (*back, with);
    }
}

TEST (PilotOrder, AFreshOrderAsksNoGame)
{
  PilotOrder const order;
  EXPECT_FALSE (order.game.has_value ());
  EXPECT_EQ (order.target.kind, PilotTargetKind::Nearest);
  EXPECT_EQ (order.with, PilotRecruit::Self);
}
