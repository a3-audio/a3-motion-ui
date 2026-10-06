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

#include <a3-motion-engine/Channel.hh>
#include <a3-motion-ui/components/ChannelValueReset.hh>
using namespace a3;

// Twelve o'clock for the two that have a middle. The knobs sweep 270 degrees,
// so the middle of the travel is the middle of the range, and a hand reaching
// for "put that back" reaches for the same place on both.
TEST (ChannelValueReset, ThreeDAndFreqRestAtTwelveOClock)
{
  for (auto const pot : { ChannelPot::ThreeD, ChannelPot::Freq })
    {
      auto const rest = channelPotRestPosition (pot);
      ASSERT_TRUE (rest.has_value ()) << channelPotLabel (pot);
      EXPECT_FLOAT_EQ (*rest, 0.5f) << channelPotLabel (pot);
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
  auto const rest = channelPotRestPosition (ChannelPot::Q);
  ASSERT_TRUE (rest.has_value ());
  EXPECT_FLOAT_EQ (*rest, 0.f);
}

// A pot nobody has decided about writes nothing. The way this goes wrong is a
// fourth pot being added and inheriting whichever value the switch happened
// to fall through to.
TEST (ChannelValueReset, APotWithNoRestPositionWritesNothing)
{
  EXPECT_FALSE (
      channelPotRestPosition (static_cast<ChannelPot> (numChannelPots))
          .has_value ());
}

// Two taps are a turn to the rest position, panel or no panel (asked for on
// the device, 2026-10-06). The reset used to be refused while the panel
// answered, on the grounds that the physical 3d pot would disagree with the
// screen -- but a drag on the same knob was never refused, so the screen could
// move a value and not put it back. Freq and Q sit on endless encoders and
// never disagree; the 3d pot disagrees after a reset exactly as it does after
// a drag, until it is next moved.
//
// Read from the source, as DeviceHello does: resetChannelPot lives in the
// component that owns the engine, and what matters is that it takes the one
// road a turn takes -- setChannelPotValue, which writes the engine (and so
// /channel/{ch}/3d, filter/frequency, filter/q on the next tick), saves the
// set and redraws every knob showing the value.
TEST (ChannelValueReset, TwoTapsAreATurnToTheRestPositionPanelOrNot)
{
  auto const ui = juce::File (__FILE__)
                      .getParentDirectory ()
                      .getParentDirectory ()
                      .getSiblingFile ("a3-motion-ui")
                      .getChildFile ("components/A3MotionUIComponent.cc");
  auto const text = ui.loadFileAsString ();
  ASSERT_TRUE (text.isNotEmpty ()) << ui.getFullPathName ();

  auto const body
      = text.fromFirstOccurrenceOf ("A3MotionUIComponent::resetChannelPot (",
                                    false, false)
            .upToFirstOccurrenceOf ("\n}\n", false, false);
  ASSERT_TRUE (body.isNotEmpty ());

  EXPECT_TRUE (body.contains ("channelPotRestPosition (pot)"));
  EXPECT_TRUE (body.contains ("setChannelPotValue (channel, pot, *rest)"));
  EXPECT_FALSE (body.contains ("hardwareIsAvailable"));
  EXPECT_FALSE (body.contains ("return;"));
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

  auto const restOf = [] (ChannelPot pot) {
    auto const rest = channelPotRestPosition (pot);
    EXPECT_TRUE (rest.has_value ()) << channelPotLabel (pot);
    return rest.value_or (-1.f);
  };

  EXPECT_FLOAT_EQ (channel.getPot3 (), restOf (ChannelPot::ThreeD));
  EXPECT_FLOAT_EQ (channel.getPot1 (), restOf (ChannelPot::Freq));
  EXPECT_FLOAT_EQ (channel.getPot2 (), restOf (ChannelPot::Q));
}

