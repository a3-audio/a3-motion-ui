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

#include "BeatTrace.hh"

#include <cstdlib>
#include <ctime>

namespace a3
{

namespace
{
long long
microsecondsOn (clockid_t clock)
{
  timespec ts{};
  clock_gettime (clock, &ts);
  return static_cast<long long> (ts.tv_sec) * 1000000LL + ts.tv_nsec / 1000;
}
}

BeatTrace::BeatTrace (std::string const &path)
{
  if (path.empty ())
    return;

  _file = std::fopen (path.c_str (), "a");
  if (_file == nullptr)
    return;

  std::fprintf (_file,
                "# monotonic_us,event,beat,bar,bpm "
                "realtime_minus_monotonic_us=%lld\n",
                microsecondsOn (CLOCK_REALTIME)
                    - microsecondsOn (CLOCK_MONOTONIC));
  std::fflush (_file);
}

BeatTrace::~BeatTrace ()
{
  if (_file != nullptr)
    std::fclose (_file);
}

bool
BeatTrace::isEnabled () const
{
  return _file != nullptr;
}

void
BeatTrace::record (char const *event, int beat, int bar, float bpm)
{
  if (_file == nullptr)
    return;

  // Stamped before the lock, so waiting for another thread's line does not
  // move this one.
  auto const now = microsecondsOn (CLOCK_MONOTONIC);

  std::lock_guard<std::mutex> lock (_mutex);
  std::fprintf (_file, "%lld,%s,%d,%d,%.3f\n", now, event, beat, bar,
                static_cast<double> (bpm));
  std::fflush (_file);
}

BeatTrace &
BeatTrace::device ()
{
  static BeatTrace trace ([] {
    auto const *path = std::getenv ("A3_BEAT_TRACE");
    return std::string (path != nullptr ? path : "");
  }());
  return trace;
}

}
