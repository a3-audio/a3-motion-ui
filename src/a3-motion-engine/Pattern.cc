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

#include "Pattern.hh"

namespace a3
{

// TODO default-initializing to channel 0 is not clean. Needs to be
// redesigned. Patterns should be channel-agnostic to begin with.
Pattern::Pattern () : _channel (0) {}

void
Pattern::clear ()
{
  std::fill (_ticks.begin (), _ticks.end (), Pos::invalid);
}

void
Pattern::resize (index_t lengthTicks)
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _ticks.resize (lengthTicks, Pos::invalid);
}

void
Pattern::setStatus (Status status)
{
  _statusLast = _status.exchange (status);
}

Pattern::Status
Pattern::getStatus () const
{
  return _status;
}

Pattern::Status
Pattern::getLastStatus () const
{
  return _statusLast;
}

void
Pattern::restoreStatus ()
{
  _status.exchange (_statusLast);
}

void
Pattern::setChannel (index_t channel)
{
  _channel = channel;
}

index_t
Pattern::getChannel () const
{
  return _channel;
}

index_t
Pattern::getNumTicks () const
{
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _ticks.size ();
}

Pos
Pattern::getTick (index_t tick) const
{
  jassert (tick < _ticks.size ());
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return _ticks[tick];
}

void
Pattern::setTick (index_t tick, Pos position)
{
  jassert (tick < _ticks.size ());
  std::lock_guard<std::mutex> guard (_ticksMutex);
  _ticks[tick] = position;
  _lastUpdatedTick = tick;
}

Pattern::Ticks
Pattern::getTicks () const
{
  // for now we just lock and return a copy while benchmarking and
  // thinking of a better solution.
  std::lock_guard<std::mutex> guard (_ticksMutex);
  return { _ticks, _lastUpdatedTick };
}

}
