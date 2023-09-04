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

#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class Pattern
{
public:
  enum class Status
  {
    Empty,
    Idle,
    ScheduledForRecording,
    Recording,
    ScheduledForPlaying,
    Playing,
  };

  void clear ();
  void resize (index_t lengthBeats);

  void setStatus (Status status);
  Status getStatus ();

private:
  std::atomic<Status> _status = Status::Idle;
  static_assert (std::atomic<Status>::is_always_lock_free);

  std::vector<Pos> _ticks;
};

}
