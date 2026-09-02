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

namespace a3
{

/** The OSC addresses this device speaks.
 *
 *  Defaults are what the system has always used, so a config without an
 *  `oscAddresses` block behaves exactly as before. `{ch}` stands for the
 *  channel number and is substituted by withChannel().
 *
 *  Changing one of these only changes *this* side of the conversation:
 *  `beat` has to match what the beat-analyzer sends, the channel addresses
 *  what A3 Core listens for. A typo here does not fail loudly — it sends
 *  correctly to an address nobody subscribes to. */
struct OscAddresses
{
  // Outgoing, to A3 Core (SpatBackendA3).
  juce::String channelAzimuth{ "/channel/{ch}/azimuth" };
  juce::String channelElevation{ "/channel/{ch}/elevation" };
  juce::String channelPot1{ "/channel/{ch}/pot_1" };
  juce::String channelPot2{ "/channel/{ch}/pot_2" };
  /** The third per-channel value ("3d"). Deliberately not
   *  `/channel/{ch}/3d`: Core dispatches on the last path element, and `3d`
   *  there is a toggle that fires on the value 1 — a continuous value would
   *  flip it whenever it passed 1.0 and do nothing the rest of the time. */
  juce::String channelPot3{ "/channel/{ch}/pot_3" };

  // Outgoing, to an IEM plugin chain (SpatBackendIEM).
  juce::String iemAzimuth{ "/StereoEncoder/azimuth" };
  juce::String iemElevation{ "/StereoEncoder/elevation" };

  /** The beat clock going out — sent every beat in INT mode. */
  juce::String beatOut{ "/beat" };
  juce::String tap{ "/tap" };
  juce::String clockMode{ "/clockmode" };

  // Incoming. vuPrefix is matched with startsWith and the channel number
  // read off what follows it.
  juce::String vuPrefix{ "/vu/" };
  juce::String energyRms{ "/EnergyVisualizer/RMS" };
  /** The beat clock coming in — followed in EXT and PIO mode.
   *
   *  Separate from beatOut, and both default to `/beat`: which one is in
   *  use follows the clock mode, so the two appear under `out` and `in`
   *  where a reader looks for them. They have to agree with whatever is at
   *  the other end; nothing here can check that. */
  juce::String beatIn{ "/beat" };
};

/** Whether juce::OSCMessage will accept this as an address.
 *
 *  It has to be asked, because JUCE throws OSCFormatError on one it will
 *  not take — and these addresses are typed on the device. Mirrors JUCE's
 *  own rule for OSCAddressPattern (see juce_OSCAddress.cpp): not empty, a
 *  leading slash, and every '/'-separated token printable ASCII without a
 *  space or a '#'. */
bool isSendableOscAddress (juce::String const &address);

/** Substitutes `{ch}` with the channel number. A pattern without the
 *  placeholder comes back unchanged. */
juce::String withChannel (juce::String const &pattern, int channel);

/** Reads the `oscAddresses` block. A key that is absent, or holds an
 *  address JUCE would refuse, keeps its default. */
OscAddresses loadOscAddresses (juce::var const &config);

}
