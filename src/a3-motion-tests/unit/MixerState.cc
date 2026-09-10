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

#include <vector>

#include <a3-motion-ui/components/MixerState.hh>

using namespace a3;

namespace
{
struct Sent
{
  juce::String address;
  float value;
};

// A state with a notebook instead of a socket, so what it would have sent is
// readable.
struct Recording
{
  MixerState state;
  std::vector<Sent> sent;

  // MixerState refuses to send to an address nobody has configured -- see
  // MixerState::send -- so a rig that wants to observe a touch actually
  // sending has to stand in for whoever would normally wire these up from
  // OscAddresses. The address text itself is not what these tests are about.
  Recording ()
  {
    state.onSend = [this] (juce::String const &address, float value) {
      sent.push_back ({ address, value });
    };
    state.channelAddress
        = [] (int, MixerControl) { return juce::String ("/test/channel"); };
    state.masterAddress
        = [] (MasterControl) { return juce::String ("/test/master"); };
    state.filterAddress
        = [] (FilterControl) { return juce::String ("/test/filter"); };
  }
};
}

// A finger sets and sends. This is the ordinary case and the only one in part
// one of the design.
TEST (MixerState, ATouchSetsTheValueAndSendsIt)
{
  Recording rig;
  rig.state.setChannelFromTouch (2, MixerControl::EqHigh, 0.75f);

  EXPECT_FLOAT_EQ (rig.state.channelValue (2, MixerControl::EqHigh), 0.75f);
  ASSERT_EQ (rig.sent.size (), 1u);
  EXPECT_FLOAT_EQ (rig.sent[0].value, 0.75f);
}

// A value from the other end sets and does **not** send. This is the whole
// provision for part two: without it, Core reports a value, Motion sets it,
// Motion sends it, Core reports it again -- the loop a3_core_echo.py exists to
// suppress, rebuilt from this side.
TEST (MixerState, AValueFromThePeerSetsAndSendsNothing)
{
  Recording rig;
  rig.state.setChannelFromPeer (2, MixerControl::EqHigh, 0.75f);

  EXPECT_FLOAT_EQ (rig.state.channelValue (2, MixerControl::EqHigh), 0.75f);
  EXPECT_TRUE (rig.sent.empty ());
}

// Nothing at all until something happens. A device that pulled four gains to
// its own defaults while coming up could ruin an evening in progress, and
// coming up is exactly when nothing is known.
TEST (MixerState, NothingIsSentBeforeAnythingIsTouched)
{
  Recording rig;
  EXPECT_TRUE (rig.sent.empty ());
}

// Toggles are values too, so one path holds all of them -- but they read back
// as what they are.
TEST (MixerState, AToggleReadsBackAsABoolean)
{
  Recording rig;
  EXPECT_FALSE (rig.state.channelToggle (0, MixerControl::Pfl));

  rig.state.setChannelFromTouch (0, MixerControl::Pfl, 1.f);
  EXPECT_TRUE (rig.state.channelToggle (0, MixerControl::Pfl));
  ASSERT_EQ (rig.sent.size (), 1u);
  EXPECT_FLOAT_EQ (rig.sent[0].value, 1.f);
}

// The master section and the filter go the same two ways, or part two would
// have to remember which of the three kinds is special.
TEST (MixerState, TheMasterAndTheFilterHaveBothPathsToo)
{
  Recording rig;
  rig.state.setMasterFromPeer (MasterControl::Booth, 0.3f);
  rig.state.setFilterFromPeer (FilterControl::Frequency, 0.4f);
  EXPECT_TRUE (rig.sent.empty ());

  EXPECT_FLOAT_EQ (rig.state.masterValue (MasterControl::Booth), 0.3f);
  EXPECT_FLOAT_EQ (rig.state.filterValue (FilterControl::Frequency), 0.4f);

  rig.state.setMasterFromTouch (MasterControl::Volume, 0.9f);
  EXPECT_EQ (rig.sent.size (), 1u);
}

// The filter's mode is a word on the wire and a state on the screen. Held as
// the value everything else is held as, read back as the question that is
// actually asked.
TEST (MixerState, TheFilterModeReadsBackAsHighOrLow)
{
  Recording rig;
  EXPECT_FALSE (rig.state.filterIsHighPass ());

  rig.state.setFilterFromTouch (FilterControl::Mode, 1.f);
  EXPECT_TRUE (rig.state.filterIsHighPass ());
}

// Every value starts in the middle except the two that must not: a gain and a
// volume at half is a guess about somebody's system, but zero is silence,
// which is the one starting point that cannot be wrong in the direction that
// matters.
TEST (MixerState, GainAndVolumeStartAtZeroAndTheRestInTheMiddle)
{
  MixerState state;
  for (auto channel = 0; channel < numChannelsInitial; ++channel)
    {
      EXPECT_FLOAT_EQ (state.channelValue (channel, MixerControl::Gain), 0.f);
      EXPECT_FLOAT_EQ (state.channelValue (channel, MixerControl::Volume),
                       0.f);
      EXPECT_FLOAT_EQ (state.channelValue (channel, MixerControl::EqMid),
                       0.5f);
    }
  EXPECT_FLOAT_EQ (state.masterValue (MasterControl::Volume), 0.f);
}

// A channel outside the four is a caller's bug, not a crash: the strip is
// built from a loop and an off-by-one there must not take the device down
// mid-set.
TEST (MixerState, AChannelOutsideTheFourIsIgnored)
{
  Recording rig;
  rig.state.setChannelFromTouch (-1, MixerControl::Gain, 1.f);
  rig.state.setChannelFromTouch (numChannelsInitial, MixerControl::Gain, 1.f);

  EXPECT_TRUE (rig.sent.empty ());
  EXPECT_FLOAT_EQ (rig.state.channelValue (-1, MixerControl::Gain), 0.f);
}

// Clamped, because a drag accumulates and nothing else stops it.
TEST (MixerState, ValuesAreClampedToTheirRange)
{
  Recording rig;
  rig.state.setChannelFromTouch (0, MixerControl::Gain, 1.5f);
  EXPECT_FLOAT_EQ (rig.state.channelValue (0, MixerControl::Gain), 1.f);

  rig.state.setChannelFromTouch (0, MixerControl::Gain, -0.5f);
  EXPECT_FLOAT_EQ (rig.state.channelValue (0, MixerControl::Gain), 0.f);
}
