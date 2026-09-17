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

#include <a3-motion-engine/util/Types.hh>

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace a3
{

/** What a take actually receives, tick by tick, for measuring.
 *
 *  Written because two takes came back with whole stretches of a smoothly
 *  drawn spiral missing (2026-09-17), and the file on disk cannot say why:
 *  it is downsampled, mirrored and cut into segments long after the fact.
 *  This is the tick array itself -- which index was written, with what, and
 *  how far the take was through when it happened -- plus the finished array
 *  with its holes still in it.
 *
 *  Off unless $A3_REC_TRACE names a file. Written from the clock thread, so
 *  only for a diagnostic run. */
class RecordingTrace
{
public:
  explicit RecordingTrace (std::string const &path);
  ~RecordingTrace ();

  RecordingTrace (RecordingTrace const &) = delete;
  RecordingTrace &operator= (RecordingTrace const &) = delete;

  bool isEnabled () const;

  /** One tick written into the take. */
  void wrote (int tick, float x, float y, bool fingerDown,
              long long ticksSinceStart);

  /** The take as it stands when the recording ends. */
  void finished (std::string const &name, std::vector<Pos> const &ticks);

  static RecordingTrace &device ();

private:
  std::mutex _mutex;
  std::FILE *_file = nullptr;
};

}
