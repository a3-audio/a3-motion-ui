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

#include <vector>

namespace a3
{

// A channel's position goes to A3 Core at most kSendsPerSecond times a
// second (2026-10-02). Sent on every clock tick it was ~260, and Core's port
// queue filled under load: desk commands waited behind stale positions.
//
// A schedule, not a stopwatch: the next send is due one interval after the
// last one was due, so ticks every 3.85 ms still give 60 a second rather than
// one per five ticks (52). After a pause the schedule restarts from now,
// instead of catching up in a burst. Pure; the caller passes the clock.
class PositionPacer
{
public:
  static constexpr double kSendsPerSecond = 60.;
  static constexpr double kIntervalMillis = 1000. / kSendsPerSecond;

  explicit PositionPacer (std::size_t channels) : _next (channels, 0.) {}

  bool
  due (std::size_t channel, double nowMillis) const
  {
    return nowMillis >= _next[channel];
  }

  void
  sent (std::size_t channel, double nowMillis)
  {
    auto &next = _next[channel];
    next = nowMillis - next > kIntervalMillis ? nowMillis + kIntervalMillis
                                              : next + kIntervalMillis;
  }

private:
  std::vector<double> _next;
};

}
