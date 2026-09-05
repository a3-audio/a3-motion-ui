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

#include <JuceHeader.h>

#include <cstddef>

namespace a3
{

/** When to give up on an open serial port and open it again.
 *
 *  `serialInit()` used to run once, in the constructor, and nothing ever
 *  asked again. Two things followed from that, and both were real:
 *
 *  - A controller plugged in after the app started was never found. Not an
 *    edge case on a device that boots with everything on one switch and
 *    whose USB-serial bridge can appear a moment late.
 *  - A link that went quiet stayed quiet for the rest of the session, with
 *    nothing to notice it and no way back but restarting the app.
 *
 *  Two rules, kept here rather than as counters in the poll loop, because
 *  both are guesses about timing and a guess you can check is worth more
 *  than a guess in the middle of a read.
 *
 *  What this does *not* fix, and was mistakenly written up as fixing: a
 *  controller whose firmware is not answering at all. That looks identical
 *  from here — an open port and no bytes — and reopening it changes nothing.
 *  The way to tell them apart is whether the device ever re-enumerates
 *  (`stat -c %z /dev/ttyACM0`, the ctime, not the mtime: the mtime moves on
 *  every write *we* make).
 */
/** Whether a reply to PING came from the controller.
 *
 *  serialInit() walks /dev/ttyACM0..2 and ttyUSB0..2 and used to take the
 *  first one that would open. On this device that is the CH343 bridge sitting
 *  beside the controller: it opens perfectly and answers nothing, so the
 *  adapter clamped onto it and read into the void for a whole session while
 *  the real controller -- native USB CDC on the ESP32-S3, a different node
 *  entirely -- was never looked at again.
 *
 *  PING (0x01) is answered with 0x01. One byte each way, in the firmware
 *  since the protocol existed (firmware/src/protocol.cpp); nothing was asking
 *  it. A port that will not say this is somebody else's port.
 */
bool isControllerPingReply (juce::uint8 const *reply, std::size_t length);

class SerialReconnect
{
public:
  /** A quarter of a second each (the read timeout), so this is about two
   *  seconds of silence. Long enough that a lost frame or a busy moment
   *  passes through it, short enough that a dead link is back before anybody
   *  reaches for a pad twice. */
  static constexpr int quietPollsBeforeReopen = 8;

  /** How often to look for a port when there is none. On a clock rather than
   *  a count: the poll loop runs about a thousand times a second, and opening
   *  a port that is not there is a syscall each time. */
  static constexpr juce::int64 retryOpeningEveryMs = 2000;

  /** A poll came back with nothing. True when it is time to reopen — and
   *  asking resets the count, so a link that stays dead asks every two
   *  seconds rather than on every poll from then on. */
  bool noteQuietPoll ();

  /** A frame arrived. */
  void noteFrameReceived () { _quietPolls = 0; }

  /** There is no port open. True when enough time has passed to try again;
   *  the first call always says yes, since that is the attempt that finds a
   *  controller which was there all along. */
  bool shouldRetryOpening (juce::int64 nowMs);

private:
  int _quietPolls = 0;
  juce::int64 _lastAttemptMs = -1;
};

}
