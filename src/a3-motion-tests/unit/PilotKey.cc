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

#include <a3-motion-ui/components/fpv/PilotKey.hh>

using namespace a3;

// The pilots' level on its key, and the one rule for the DJ taking over.

TEST (PilotKey, TheKeySaysTheLevel)
{
  auto const off = pilotKeyFace (PilotLevel::Off, true);
  EXPECT_EQ (off.word, "PILOTS");
  EXPECT_FALSE (off.on);
  EXPECT_FALSE (off.notice);

  auto const hint = pilotKeyFace (PilotLevel::Hint, true);
  EXPECT_EQ (hint.word, "HINT");
  EXPECT_TRUE (hint.on);
  EXPECT_FALSE (hint.notice);

  auto const fly = pilotKeyFace (PilotLevel::Fly, true);
  EXPECT_EQ (fly.word, "FLY");
  EXPECT_TRUE (fly.on);
  EXPECT_TRUE (fly.notice) << "someone else is driving";
}

TEST (PilotKey, FullShowsTheLevelDimmedAndQuiet)
{
  for (auto const level : { PilotLevel::Off, PilotLevel::Hint, PilotLevel::Fly })
    {
      auto const face = pilotKeyFace (level, false);
      EXPECT_FALSE (face.available) << pilotLevelWord (level);
      EXPECT_FALSE (face.on) << pilotLevelWord (level);
      EXPECT_FALSE (face.notice) << pilotLevelWord (level);
      EXPECT_EQ (face.word, pilotKeyFace (level, true).word);
    }
}

TEST (PilotKey, AtFlyADjsGameOrATouchedPilotsShipTakesOver)
{
  EXPECT_EQ (levelAfterTap (PilotLevel::Fly, true, false), PilotLevel::Hint);
  EXPECT_EQ (levelAfterTap (PilotLevel::Fly, false, true), PilotLevel::Hint);
  EXPECT_EQ (levelAfterTap (PilotLevel::Fly, true, true), PilotLevel::Hint);
  EXPECT_EQ (levelAfterTap (PilotLevel::Fly, false, false), PilotLevel::Fly)
      << "an action on a ship no pilot is playing with leaves FLY alone";
}

TEST (PilotKey, BelowFlyATapChangesNoLevel)
{
  for (auto const level : { PilotLevel::Off, PilotLevel::Hint })
    EXPECT_EQ (levelAfterTap (level, true, true), level);
}

TEST (PilotKey, TheReadoutsSayItInCapitals)
{
  EXPECT_EQ (pilotLevelReadout (PilotLevel::Hint), "-- PILOTS HINT");
  EXPECT_EQ (pilotLevelReadout (PilotLevel::Off), "-- PILOTS OFF");
  EXPECT_EQ (pilotGameReadout (1, PilotGame::FakeOut), "CH2 FLY FAKEOUT");
  EXPECT_EQ (pilotGameReadout (3, PilotGame::CallAndResponse), "CH4 FLY CALLRESPONSE");
}
