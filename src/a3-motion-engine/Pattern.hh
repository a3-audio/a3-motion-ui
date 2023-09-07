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
    ScheduledForIdle,
    Idle,
    ScheduledForRecording,
    Recording,
    ScheduledForPlaying,
    Playing,
  };

  Pattern ();

  void clear ();
  void resize (index_t lengthBeats);

  void setStatus (Status status);
  Status getStatus () const;

  Status getLastStatus () const;
  void restoreStatus ();

  void setChannel (index_t channel);
  index_t getChannel () const;

  index_t getNumTicks () const;
  std::vector<Pos> getTicks () const;
  Pos getTick (index_t tick) const;
  void setTick (index_t tick, Pos position);

private:
  static_assert (std::atomic<Status>::is_always_lock_free);
  std::atomic<Status> _status = Status::Empty;
  std::atomic<Status> _statusLast = Status::Empty;

  // for now patterns are fixed to a channel, this will probably
  // change later on.
  std::atomic<index_t> _channel;

  std::vector<Pos> _ticks;
  mutable std::mutex _ticksMutex;
};

}
