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

// Plain Play|Pause is now; Shift waits for the downbeat; two plain taps in
// quick succession go back to the top (maintainer, 2026-10-08).

TEST (PlayPausePress, APlainPressActsAtOnce)
{
  EXPECT_EQ (playPausePress (Status::Playing, false),
             PlayPausePress::PauseNow);
  EXPECT_EQ (playPausePress (Status::Idle, false), PlayPausePress::StartNow);
}

TEST (PlayPausePress, ShiftWaitsForTheDownbeat)
{
  EXPECT_EQ (playPausePress (Status::Playing, true),
             PlayPausePress::PauseOnTheDownbeat);
  // Paused or stopped alike: the engine knows which, and goes on or starts.
  EXPECT_EQ (playPausePress (Status::Idle, true),
             PlayPausePress::StartOnTheDownbeat);
}

TEST (PlayPausePress, AWaitingStartIsCalledOffByAnyPress)
{
  for (auto const shift : { false, true })
    EXPECT_EQ (playPausePress (Status::ScheduledForPlaying, shift),
               PlayPausePress::CancelStart);
}

TEST (PlayPausePress, AWaitingPauseIsLeftToLand)
{
  for (auto const shift : { false, true })
    EXPECT_EQ (playPausePress (Status::ScheduledForIdle, shift),
               PlayPausePress::Nothing);
}

// The first tap already did its plain thing, so the second finds the
// opposite status and turns it into the way back to the top.
TEST (PlayPausePress, ASecondTapOnAPausedClipStopsItToTheTop)
{
  EXPECT_EQ (playPausePress (Status::Idle, false, { 200, false }),
             PlayPausePress::StopToTheTop);
}

TEST (PlayPausePress, ASecondTapOnAResumedClipStartsItFromTheTopNow)
{
  EXPECT_EQ (playPausePress (Status::Playing, false, { 200, false }),
             PlayPausePress::StartFromTheTopNow);
}

TEST (PlayPausePress, TheWindowHasAnEdge)
{
  EXPECT_EQ (playPausePress (Status::Idle, false,
                             { doubleTapWindowMs, false }),
             PlayPausePress::StopToTheTop);
  EXPECT_EQ (playPausePress (Status::Idle, false,
                             { doubleTapWindowMs + 1, false }),
             PlayPausePress::StartNow);
  EXPECT_EQ (playPausePress (Status::Playing, false,
                             { doubleTapWindowMs + 1, false }),
             PlayPausePress::PauseNow);
}

// A Shift press scheduled something; the tap after it is no double tap.
TEST (PlayPausePress, AShiftPressBeforeOrInTheTapIsNoDoubleTap)
{
  EXPECT_EQ (playPausePress (Status::Idle, false, { 200, true }),
             PlayPausePress::StartNow);
  EXPECT_EQ (playPausePress (Status::Idle, true, { 200, false }),
             PlayPausePress::StartOnTheDownbeat);
  EXPECT_FALSE (isDoubleTap (true, { 200, false }));
  EXPECT_FALSE (isDoubleTap (false, { 200, true }));
  EXPECT_TRUE (isDoubleTap (false, { 200, false }));
}

// Under X one touch arrives twice a few milliseconds apart. That must not
// toggle twice, nor count as a double tap.
TEST (PlayPausePress, ATwinOfTheSameTouchDoesNothing)
{
  for (auto const status : { Status::Idle, Status::Playing,
                             Status::ScheduledForPlaying })
    for (auto const shift : { false, true })
      EXPECT_EQ (playPausePress (status, shift, { 3, shift }),
                 PlayPausePress::Nothing)
          << static_cast<int> (status) << " shift " << shift;
  EXPECT_FALSE (isDoubleTap (false, { 3, false }));
  EXPECT_EQ (playPausePress (Status::Idle, false,
                             { twinWindowMs, false }),
             PlayPausePress::StopToTheTop)
      << "a human double tap is never this fast; the edge is a tap";
}

TEST (PlayPausePress, TheWindowsDoNotOverlap)
{
  EXPECT_LT (twinWindowMs, doubleTapWindowMs);
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
