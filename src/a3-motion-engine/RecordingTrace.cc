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

#include "RecordingTrace.hh"

#include <cstdlib>

namespace a3
{

RecordingTrace::RecordingTrace (std::string const &path)
{
  if (path.empty ())
    return;

  _file = std::fopen (path.c_str (), "a");
  if (_file == nullptr)
    return;

  std::fprintf (_file, "# tick,x,y,fingerDown,ticksSinceStart\n");
  std::fflush (_file);
}

RecordingTrace::~RecordingTrace ()
{
  if (_file != nullptr)
    std::fclose (_file);
}

bool
RecordingTrace::isEnabled () const
{
  return _file != nullptr;
}

void
RecordingTrace::wrote (int tick, float x, float y, bool fingerDown,
                       long long ticksSinceStart)
{
  if (_file == nullptr)
    return;

  std::lock_guard<std::mutex> lock (_mutex);
  std::fprintf (_file, "%d,%.4f,%.4f,%d,%lld\n", tick, static_cast<double> (x),
                static_cast<double> (y), fingerDown ? 1 : 0, ticksSinceStart);
  std::fflush (_file);
}

void
RecordingTrace::finished (std::string const &name, std::vector<Pos> const &ticks)
{
  if (_file == nullptr)
    return;

  std::lock_guard<std::mutex> lock (_mutex);
  std::fprintf (_file, "# finished %s ticks=%d\n", name.c_str (),
                static_cast<int> (ticks.size ()));
  for (std::size_t i = 0; i < ticks.size (); ++i)
    {
      auto const &pos = ticks[i];
      auto const valid = pos.isValid ();
      std::fprintf (_file, "final,%d,%.4f,%.4f,%d\n", static_cast<int> (i),
                    valid ? static_cast<double> (pos.x ()) : 0.0,
                    valid ? static_cast<double> (pos.y ()) : 0.0,
                    valid ? 1 : 0);
    }
  std::fflush (_file);
}

RecordingTrace &
RecordingTrace::device ()
{
  static RecordingTrace trace ([] {
    auto const *path = std::getenv ("A3_REC_TRACE");
    return std::string (path != nullptr ? path : "");
  }());
  return trace;
}

}
