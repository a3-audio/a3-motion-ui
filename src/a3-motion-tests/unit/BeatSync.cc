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

#include <a3-motion-engine/tempo/BeatSync.hh>

#include <gtest/gtest.h>

namespace a3
{

TEST (BeatSync, OnTheBeatNothingMoves)
{
  EXPECT_DOUBLE_EQ (beatSyncShift (2.0, 2, 4), 0.0);
}

TEST (BeatSync, ASmallLagIsPulledInGently)
{
  // Measured 2026-09-17: the engine drifts about ten milliseconds a beat
  // against the beat it is sent. Pulled in by a share each beat, that is a
  // correction nobody hears; pulled in whole, every beat would be a jolt.
  auto const shift = beatSyncShift (0.96, 1, 4);
  EXPECT_GT (shift, 0.0);
  EXPECT_LT (shift, 0.04);
}

TEST (BeatSync, ASmallLeadIsHeldBackGently)
{
  auto const shift = beatSyncShift (1.04, 1, 4);
  EXPECT_LT (shift, 0.0);
  EXPECT_GT (shift, -0.04);
}

TEST (BeatSync, TheBarLineIsNotAWholeBarAway)
{
  // Just before the downbeat and the one arriving are a hair apart, not a
  // bar: the error is taken the short way round.
  auto const shift = beatSyncShift (3.98, 0, 4);
  EXPECT_GT (shift, 0.0);
  EXPECT_LT (shift, 0.02);
}

TEST (BeatSync, FarOffItJumpsTheWholeWay)
{
  // Off by more than the lock window the engine is on the wrong beat, and
  // easing there over many beats is many beats out of time. One jump, the
  // short way round.
  EXPECT_DOUBLE_EQ (beatSyncShift (3.5, 0, 4), 0.5);
  EXPECT_DOUBLE_EQ (beatSyncShift (1.5, 0, 4), -1.5);
  // Exactly half a bar either way: forwards, so the engine never stands
  // still for two beats when it could catch up instead.
  EXPECT_DOUBLE_EQ (beatSyncShift (2.0, 0, 4), 2.0);
}

TEST (BeatSync, WhereTheDownbeatIsComesFromTheSource)
{
  // On the right beat of the wrong place in the bar is still wrong: the
  // "one" is what a clip starting on a downbeat waits for.
  EXPECT_DOUBLE_EQ (beatSyncShift (1.0, 3, 4), 2.0);
}

TEST (BeatSync, ABeatNumberOutsideTheBarIsFoldedIn)
{
  EXPECT_DOUBLE_EQ (beatSyncShift (1.0, 5, 4), 0.0);
  EXPECT_DOUBLE_EQ (beatSyncShift (1.0, -3, 4), 0.0);
}

}
