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

#include <cmath>

#include <gtest/gtest.h>

namespace a3
{

namespace
{
BeatSyncCorrection
firstBeat (double position, int arriving, int beatsPerBar = 4)
{
  BeatPhaseFollower follower;
  return follower.onBeat (position, arriving, beatsPerBar);
}
}

TEST (BeatSync, OnTheBeatNothingMoves)
{
  auto const c = firstBeat (2.0, 2);
  EXPECT_DOUBLE_EQ (c.timeShift, 0.0);
  EXPECT_EQ (c.beatsToAdd, 0);
}

TEST (BeatSync, ASmallLagIsPulledInGently)
{
  // Measured 2026-09-17: the engine drifts about ten milliseconds a beat
  // against the beat it is sent. Pulled in by a share each beat, that is a
  // correction nobody hears; pulled in whole, every beat would be a jolt.
  auto const shift = firstBeat (0.96, 1).timeShift;
  EXPECT_GT (shift, 0.0);
  EXPECT_LT (shift, 0.04);
}

TEST (BeatSync, ASmallLeadIsHeldBackGently)
{
  auto const shift = firstBeat (1.04, 1).timeShift;
  EXPECT_LT (shift, 0.0);
  EXPECT_GT (shift, -0.04);
}

TEST (BeatSync, TheBarLineIsNotAWholeBarAway)
{
  // Just before the downbeat and the one arriving are a hair apart, not a
  // bar: the error is taken the short way round.
  auto const c = firstBeat (3.98, 0);
  EXPECT_GT (c.timeShift, 0.0);
  EXPECT_LT (c.timeShift, 0.02);
  EXPECT_EQ (c.beatsToAdd, 0);
}

// a3-motion-ui#36: past the lock window the engine used to jump the whole
// way, and an unsteady clock's outliers -- up to 400 ms at 70 BPM -- made it
// jump its phase. One beat far off is a stray now, and changes nothing.
TEST (BeatSync, OneBeatFarOffIsAStray)
{
  BeatPhaseFollower follower;
  follower.onBeat (1.0, 1, 4);
  auto const c = follower.onBeat (2.4, 2, 4);
  EXPECT_DOUBLE_EQ (c.timeShift, 0.0);
  EXPECT_EQ (c.beatsToAdd, 0);
}

// Far off twice in a row is a real offset, and it is caught up -- in steps
// no bigger than beatSyncMaxStep, never in one jump.
TEST (BeatSync, AConfirmedOffsetIsCaughtUpWithoutAJump)
{
  BeatPhaseFollower follower;
  auto lag = 0.4; // behind by this much of a beat
  follower.onBeat (1.0 - lag, 1, 4);

  for (int beat = 0; beat < 20; ++beat)
    {
      auto const c = follower.onBeat (2.0 - lag, 2, 4);
      EXPECT_LE (std::abs (c.timeShift), beatSyncMaxStep + 1e-12)
          << "beat " << beat;
      lag -= c.timeShift;
    }
  EXPECT_LT (std::abs (lag), 0.02) << "never caught up";
}

// On the wrong beat of the bar, the engine's beat is renumbered rather than
// moved: which beat it is changes, where it is in time does not -- so a
// clip plays on without a burst of ticks. Once confirmed, like any offset.
TEST (BeatSync, AWrongBeatIsRenumberedNotJumped)
{
  BeatPhaseFollower follower;
  auto const first = follower.onBeat (1.0, 3, 4);
  EXPECT_EQ (first.beatsToAdd, 0) << "renumbered on a single beat";

  auto const second = follower.onBeat (2.0, 0, 4);
  EXPECT_EQ (second.beatsToAdd, 2);
  EXPECT_NEAR (second.timeShift, 0.0, 1e-12);
}

TEST (BeatSync, ABeatNumberOutsideTheBarIsFoldedIn)
{
  EXPECT_EQ (firstBeat (1.0, 5).beatsToAdd, 0);
  EXPECT_DOUBLE_EQ (firstBeat (1.0, 5).timeShift, 0.0);
  EXPECT_DOUBLE_EQ (firstBeat (1.0, -3).timeShift, 0.0);
}

}
