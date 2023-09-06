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
Pattern::resize (index_t lengthBeats)
{
  _ticks.resize (TempoClock::Config::ticksPerBeat * lengthBeats, Pos::invalid);
}

void
Pattern::setStatus (Status status)
{
  _statusLast = _status.load ();
  _status = status;
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
  // Status statusLast = _status;
  _status = _statusLast.load ();
  // _statusLast = statusLast;
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

}
