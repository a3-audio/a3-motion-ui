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
  StartNow,           ///< at once; a paused clip goes on from its place
  StartOnTheDownbeat, ///< on the next downbeat; a paused clip goes on
  StartFromTheTopNow, ///< at once, from the top, a pause forgotten
  PauseNow,           ///< at once, keeping the place exactly
  PauseOnTheDownbeat, ///< on the next downbeat, keeping the place
  StopToTheTop,       ///< at once, back to the top
  CancelStart,        ///< a start still waiting for its downbeat, called off
  Nothing,
};

/** Two plain taps closer than this are a double tap: back to the top.
 *  350 ms is under the 400-500 ms desktops give a double click, because a
 *  tap on a pad is a shorter gesture than a click and a wider window would
 *  turn a deliberate pause-then-play into a restart; and well above the
 *  ~100 ms a thumb needs for two quick taps. */
constexpr long doubleTapWindowMs = 350;

/** Under X one touch arrives twice, as touch and emulated mouse, a couple of
 *  milliseconds apart. A second press this soon is that twin, not a hand. */
constexpr long twinWindowMs = 40;

/** The press before this one on the same clip, as the route needs it. */
struct PreviousPress
{
  long msAgo = 1L << 30; ///< far past: there was none
  bool shift = false;
};

/** Whether a press at `previous` + now is the second tap of a double tap:
 *  both plain, close together, and not the same touch twice. */
constexpr bool
isDoubleTap (bool shift, PreviousPress previous)
{
  return !shift && !previous.shift && previous.msAgo >= twinWindowMs
         && previous.msAgo <= doubleTapWindowMs;
}

/** Play|Pause, plain and with Shift (maintainer, 2026-10-08).
 *
 *  Plain acts at once: ▶ starts -- a paused clip goes on from where it stood
 *  -- and ❚❚ pauses, keeping the place. **Shift waits for the downbeat**, the
 *  key blinking meanwhile. **Two plain taps go back to the top**: the first
 *  already did its thing, so on a paused clip the second stops it to the top
 *  and on a resumed one it starts it over -- the panel has no ■ (2026-09-27),
 *  so this is its way back to the top. A press that is the twin of the
 *  previous one does nothing. */
constexpr PlayPausePress
playPausePress (Pattern::Status status, bool shift,
                PreviousPress previous = {})
{
  if (previous.msAgo < twinWindowMs)
    return PlayPausePress::Nothing;

  auto const again = isDoubleTap (shift, previous);
  switch (status)
    {
    case Pattern::Status::Idle:
      if (again)
        return PlayPausePress::StopToTheTop;
      return shift ? PlayPausePress::StartOnTheDownbeat
                   : PlayPausePress::StartNow;
    case Pattern::Status::Playing:
      if (again)
        return PlayPausePress::StartFromTheTopNow;
      return shift ? PlayPausePress::PauseOnTheDownbeat
                   : PlayPausePress::PauseNow;
    case Pattern::Status::ScheduledForIdle:
      return PlayPausePress::Nothing;
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
