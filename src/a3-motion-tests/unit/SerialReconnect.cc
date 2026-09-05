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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/io/SerialReconnect.hh>

using namespace a3;

// ── A link that has gone quiet ───────────────────────────────────────────

// One missed frame is a missed frame. The panel answers within a quarter of a
// second and the odd one gets lost; tearing the port down for that would take
// the controller away over nothing.
TEST (SerialReconnect, OneQuietPollIsNotAProblem)
{
  SerialReconnect reconnect;

  EXPECT_FALSE (reconnect.noteQuietPoll ());
}

// Enough of them in a row is: the port is open, the reads are timing out, and
// what is on the other end of the descriptor is a device that has gone. That
// is the shape of an ESP32 that re-enumerated after the port was opened, and
// nothing else is going to fix it.
TEST (SerialReconnect, EnoughOfThemInARowIs)
{
  SerialReconnect reconnect;

  auto asked = false;
  for (int poll = 0; poll < SerialReconnect::quietPollsBeforeReopen; ++poll)
    asked = reconnect.noteQuietPoll ();

  EXPECT_TRUE (asked);
}

// A frame that arrives clears the count. A link that loses one frame a second
// is a link that works, and it must never accumulate its way into a reopen.
TEST (SerialReconnect, AFrameThatArrivesClearsTheCount)
{
  SerialReconnect reconnect;

  for (int i = 0; i < 200; ++i)
    {
      for (int quiet = 0;
           quiet < SerialReconnect::quietPollsBeforeReopen - 1; ++quiet)
        EXPECT_FALSE (reconnect.noteQuietPoll ()) << "at " << i;

      reconnect.noteFrameReceived ();
    }
}

// And asking once is asking once: the caller reopens and the count starts
// again, or a still-dead link would ask on every poll from then on and the
// log would be the reopen, ten times a second.
TEST (SerialReconnect, AskingResetsTheCount)
{
  SerialReconnect reconnect;

  for (int poll = 0; poll < SerialReconnect::quietPollsBeforeReopen; ++poll)
    reconnect.noteQuietPoll ();

  EXPECT_FALSE (reconnect.noteQuietPoll ())
      << "asked twice in a row without a frame in between";
}

// ── No port at all ───────────────────────────────────────────────────────

// A controller plugged in after the app started should be found. The poll
// loop runs about a thousand times a second, so this is on a clock rather
// than a count: opening a port that is not there is a syscall per attempt,
// and a thousand of those a second is a busy loop with a log behind it.
TEST (SerialReconnect, WithNoPortItRetriesOnAClock)
{
  SerialReconnect reconnect;

  EXPECT_TRUE (reconnect.shouldRetryOpening (0))
      << "the first attempt is the one that finds a controller already there";

  EXPECT_FALSE (reconnect.shouldRetryOpening (10));
  EXPECT_FALSE (reconnect.shouldRetryOpening (
      SerialReconnect::retryOpeningEveryMs - 1));

  EXPECT_TRUE (
      reconnect.shouldRetryOpening (SerialReconnect::retryOpeningEveryMs));
  EXPECT_FALSE (reconnect.shouldRetryOpening (
      SerialReconnect::retryOpeningEveryMs + 1));
}

// A clock that goes backwards -- which is what a wrapped or corrected one
// looks like -- must not lock the retry out until it catches up again.
TEST (SerialReconnect, AClockThatGoesBackwardsDoesNotLockItOut)
{
  SerialReconnect reconnect;

  ASSERT_TRUE (reconnect.shouldRetryOpening (1000000));
  EXPECT_TRUE (reconnect.shouldRetryOpening (5));
}

// ── Telling the controller from whatever else is on the bus ──────────────

// serialInit() walks /dev/ttyACM0..2 and ttyUSB0..2 and used to take the
// first one that would open. On this device that was the CH343 bridge sitting
// beside the controller, which opens perfectly and answers nothing -- so the
// adapter clamped onto the wrong port and read into the void for the whole
// session, with the actual controller (native USB CDC, a different node
// entirely) never looked at again.
//
// A port is the controller when it answers PING with PING. That is one byte
// each way and the firmware has always had it; nothing was asking.
TEST (SerialReconnect, ThePingReplyIsTheOwnCommandBack)
{
  uint8_t const good[] = { 0x01 };
  EXPECT_TRUE (isControllerPingReply (good, 1));
}

TEST (SerialReconnect, AnythingElseIsSomebodyElsesPort)
{
  // Nothing at all: the shape of a bridge with no controller behind it.
  EXPECT_FALSE (isControllerPingReply (nullptr, 0));

  // A byte, but not ours -- noise on the line, or another protocol.
  for (uint8_t byte : { 0x00, 0x02, 0x04, 0xff })
    {
      uint8_t const reply[] = { byte };
      EXPECT_FALSE (isControllerPingReply (reply, 1))
          << "accepted 0x" << juce::String::toHexString (byte);
    }
}
