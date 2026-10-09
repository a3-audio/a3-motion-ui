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

#pragma once

#include <vector>

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-ui/components/PlayPausePress.hh>

namespace a3
{

/** Which channels a press on PLAY all reaches.
 *
 *  **The room toggles as one**: if anything plays -- or waits to start --
 *  PLAY all reaches what plays and pauses it; if nothing does, it reaches
 *  every clip that stands still and starts it. A toggle per channel would
 *  start half the room and stop the other half. A take is REC's and is never
 *  reached. Each reached channel then goes through its own Play|Pause, so a
 *  paused clip resumes and a waiting start is called off, as on its pad.
 *
 *  **Two taps go back to the top** for the channels the first tap reached,
 *  each running its own double tap; asking the rule again would pick another
 *  set, because the first tap has already turned the room over. A twin of one
 *  touch reaches the same channels too, where every Play|Pause ignores it. */
class PlayAllPress
{
public:
  std::vector<bool>
  press (std::vector<Pattern::Status> const &room, long long nowMs)
  {
    if (_lastMs != 0)
      {
        auto const msAgo = nowMs - _lastMs;
        if (msAgo < twinWindowMs)
          return _lastPicks;
        if (msAgo <= doubleTapWindowMs)
          {
            _lastMs = 0;
            return _lastPicks;
          }
      }

    _lastPicks = toggle (room);
    _lastMs = nowMs == 0 ? 1 : nowMs;
    return _lastPicks;
  }

private:
  static bool
  runs (Pattern::Status status)
  {
    return status == Pattern::Status::Playing
           || status == Pattern::Status::ScheduledForPlaying;
  }

  static std::vector<bool>
  toggle (std::vector<Pattern::Status> const &room)
  {
    auto anythingRuns = false;
    for (auto const status : room)
      anythingRuns = anythingRuns || runs (status);

    std::vector<bool> picks;
    for (auto const status : room)
      picks.push_back (anythingRuns ? runs (status)
                                    : status == Pattern::Status::Idle);
    return picks;
  }

  /** 0: no press to pair with. */
  long long _lastMs = 0;
  std::vector<bool> _lastPicks;
};

}
