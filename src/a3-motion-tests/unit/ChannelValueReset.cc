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

#include <a3-motion-engine/Channel.hh>
#include <a3-motion-ui/components/ChannelValueReset.hh>
#include <a3-motion-ui/components/ClipSettingsLayout.hh>

using namespace a3;

// Twelve o'clock for the two that have a middle. The knobs sweep 270 degrees,
// so the middle of the travel is the middle of the range, and a hand reaching
// for "put that back" reaches for the same place on both.
TEST (ChannelValueReset, ThreeDAndFreqRestAtTwelveOClock)
{
  for (auto const row : { channelRowThreeD, channelRowFreq })
    {
      auto const rest = channelValueRestPosition (row);
      ASSERT_TRUE (rest.has_value ()) << row;
      EXPECT_FLOAT_EQ (*rest, 0.5f) << row;
    }
}

// Q comes to rest shut, and that is a decision that has been made twice.
//
// It rested shut, was moved to twelve o'clock so the three knobs would look
// alike after a double tap, and was moved back on 2026-09-12 by the
// maintainer. The value wins over the gesture: a filter that still resonates
// after being put back has not been put back. At the far end is an Airwindows
// Isolator3 whose own Q rests at zero.
TEST (ChannelValueReset, QRestsShut)
{
  auto const rest = channelValueRestPosition (channelRowQ);
  ASSERT_TRUE (rest.has_value ());
  EXPECT_FLOAT_EQ (*rest, 0.f);
}

// A row nobody has decided about writes nothing. The way this goes wrong is a
// fourth row being added to the grid and inheriting whichever value the switch
// happened to fall through to.
TEST (ChannelValueReset, ARowWithNoRestPositionWritesNothing)
{
  EXPECT_FALSE (channelValueRestPosition (-1).has_value ());
  EXPECT_FALSE (channelValueRestPosition (numChannelRows).has_value ());
}

// With the panel answering, these three are physical pots and an encoder pair.
// A value that jumped away from where the pot is standing would be telling the
// truth about neither, and the next hair of pot movement would snatch it back.
TEST (ChannelValueReset, ThePanelOwnsTheValuesWhileItIsAnswering)
{
  EXPECT_FALSE (channelValueResetIsAllowed (true));
}

// Without it the screen is the only way in, so two taps are the only way back.
// "Without it" is answered at runtime, not at build time: this device is built
// with the hardware interface compiled in and still spends sessions with no
// panel on the wire.
TEST (ChannelValueReset, TheScreenMayResetWhenNoPanelAnswers)
{
  EXPECT_TRUE (channelValueResetIsAllowed (false));
}


// ── Where they start ────────────────────────────────────────────────────────

// A control starts where it comes to rest. "Where does this belong when
// nothing has said otherwise" is one question, and the three pots had three
// different answers to it from the one above: 3d rested at twelve o'clock and
// started shut, freq rested at twelve and started at a quarter, Q rested shut
// and started wide open.
//
// That is not only a picture. The device *sends* these, and A3 Core records
// what it is sent -- so a start value is written into Core's state file as
// though a hand had set it, and the next recall replays it. The maintainer
// found all four 3d pots reading zero on a rig and asked why the recall was
// not bringing the last setting back. It was: the last setting was this
// default, sent at the previous start-up.
TEST (ChannelValueReset, EachPotStartsOnItsRestPosition)
{
  Channel channel;

  auto const restOf = [] (int row) {
    auto const rest = channelValueRestPosition (row);
    EXPECT_TRUE (rest.has_value ()) << row;
    return rest.value_or (-1.f);
  };

  EXPECT_FLOAT_EQ (channel.getPot3 (), restOf (channelRowThreeD));
  EXPECT_FLOAT_EQ (channel.getPot1 (), restOf (channelRowFreq));
  EXPECT_FLOAT_EQ (channel.getPot2 (), restOf (channelRowQ));
}
