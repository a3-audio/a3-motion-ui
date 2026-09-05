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

#include "SerialReconnect.hh"

namespace a3
{

bool
isControllerPingReply (juce::uint8 const *reply, std::size_t length)
{
  // The command comes back as its own answer, so there is exactly one right
  // byte and everything else -- including silence -- is a wrong port.
  constexpr juce::uint8 ping = 0x01;

  return reply != nullptr && length == 1 && reply[0] == ping;
}

bool
SerialReconnect::noteQuietPoll ()
{
  if (++_quietPolls < quietPollsBeforeReopen)
    return false;

  _quietPolls = 0;
  return true;
}

bool
SerialReconnect::shouldRetryOpening (juce::int64 nowMs)
{
  // A clock that has gone backwards — wrapped, or corrected under us — must
  // not lock the retry out until it has caught up again.
  auto const due = _lastAttemptMs < 0 || nowMs < _lastAttemptMs
                   || nowMs - _lastAttemptMs >= retryOpeningEveryMs;

  if (!due)
    return false;

  _lastAttemptMs = nowMs;
  return true;
}

}
