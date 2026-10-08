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

#include <a3-motion-ui/components/PlayPausePress.hh>

using namespace a3;

using Status = Pattern::Status;

// ❚❚ pauses on the downbeat and keeps the place; Shift means now and from the
// top -- the panel's only way back to the top, since it has no ■ (maintainer,
// 2026-10-08).

TEST (PlayPausePress, PlayPausesARunningClipAndShiftStopsIt)
{
  EXPECT_EQ (playPausePress (Status::Playing, false),
             PlayPausePress::Pause);
  EXPECT_EQ (playPausePress (Status::Playing, true),
             PlayPausePress::StopToTheTop);
}

TEST (PlayPausePress, AStillClipStartsAndShiftStartsItFromTheTopNow)
{
  // Paused or stopped alike: the engine knows which, and goes on or starts.
  EXPECT_EQ (playPausePress (Status::Idle, false), PlayPausePress::Start);
  EXPECT_EQ (playPausePress (Status::Idle, true),
             PlayPausePress::StartFromTheTopNow);
}

// A pause waiting for its downbeat: Shift takes it to the top at once, so the
// pause cannot land after it and keep a place nobody wants.
TEST (PlayPausePress, ShiftOnAPendingPauseStopsToTheTop)
{
  EXPECT_EQ (playPausePress (Status::ScheduledForIdle, true),
             PlayPausePress::StopToTheTop);
  EXPECT_EQ (playPausePress (Status::ScheduledForIdle, false),
             PlayPausePress::Nothing);
}

TEST (PlayPausePress, AWaitingStartIsCalledOff)
{
  for (auto const shift : { false, true })
    EXPECT_EQ (playPausePress (Status::ScheduledForPlaying, shift),
               PlayPausePress::CancelStart);
}

// ■ on the screen reaches every clip that has somewhere to go back from: a
// pending pause too, which it used to miss -- the pause then landed and the
// next ▶ resumed mid-clip.
TEST (StopReachesClip, EverythingWithAPlaceToLeave)
{
  for (auto const status : { Status::Playing, Status::Recording,
                             Status::ScheduledForPlaying,
                             Status::ScheduledForIdle })
    EXPECT_TRUE (stopReachesClip (status, false))
        << static_cast<int> (status);
  EXPECT_TRUE (stopReachesClip (Status::Idle, true)) << "paused";
  EXPECT_FALSE (stopReachesClip (Status::Idle, false)) << "already at the top";
  EXPECT_FALSE (stopReachesClip (Status::Empty, false));
}
