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

#pragma once

#include <a3-motion-engine/Pattern.hh>

namespace a3
{

/** What a press on Play|Pause does to its clip. */
enum class PlayPausePress
{
  Start,              ///< on the next downbeat; a paused clip goes on
  StartFromTheTopNow, ///< at once, from the top, a pause forgotten
  Pause,              ///< on the next downbeat, keeping the place
  StopToTheTop,       ///< at once, back to the top
  CancelStart,        ///< a start still waiting for its downbeat, called off
  Nothing,
};

/** Play|Pause, plain and with Shift (maintainer, 2026-10-08).
 *
 *  Plain waits for the downbeat: ▶ starts -- a paused clip goes on from where
 *  it stood -- and ❚❚ pauses, keeping the place. **Shift means now and from
 *  the top**: on a running clip or a pause still waiting it stops and goes
 *  back to the top, on a still one it starts from the top at once. The panel
 *  has no ■ (2026-09-27), so Shift is its only way back to the top. */
constexpr PlayPausePress
playPausePress (Pattern::Status status, bool shift)
{
  switch (status)
    {
    case Pattern::Status::Idle:
      return shift ? PlayPausePress::StartFromTheTopNow
                   : PlayPausePress::Start;
    case Pattern::Status::Playing:
      return shift ? PlayPausePress::StopToTheTop : PlayPausePress::Pause;
    case Pattern::Status::ScheduledForIdle:
      return shift ? PlayPausePress::StopToTheTop : PlayPausePress::Nothing;
    case Pattern::Status::ScheduledForPlaying:
      return PlayPausePress::CancelStart;
    case Pattern::Status::Empty:
    case Pattern::Status::ScheduledForRecording:
    case Pattern::Status::Recording:
      return PlayPausePress::Nothing;
    }
  return PlayPausePress::Nothing;
}

/** Whether ■ on the screen has anything to send back to the top: a clip that
 *  runs, records or waits to start or to pause, and a paused one. A pending
 *  pause used to be missed, so it landed after ■ and the next ▶ resumed
 *  mid-clip. */
constexpr bool
stopReachesClip (Pattern::Status status, bool paused)
{
  switch (status)
    {
    case Pattern::Status::Playing:
    case Pattern::Status::Recording:
    case Pattern::Status::ScheduledForPlaying:
    case Pattern::Status::ScheduledForIdle:
      return true;
    case Pattern::Status::Idle:
      return paused;
    case Pattern::Status::Empty:
    case Pattern::Status::ScheduledForRecording:
      return false;
    }
  return false;
}

}
