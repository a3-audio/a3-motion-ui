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

#include <array>

#include <JuceHeader.h>

namespace a3
{

/** How many addresses each of OscAddresses's mixer tables holds.
 *
 *  These mirror MixerControls.hh's numMixerControls/numMasterControls/
 *  numFilterControls, but are the engine's own constants rather than a
 *  reference to that header: the engine has `src/` on its include path, so
 *  `#include <a3-motion-ui/...>` would resolve, but the engine pulls
 *  nothing from the ui today and a table of three counts is not a reason to
 *  start. The two sides agreeing is therefore not something the compiler
 *  can check — it is what
 *  OscAddresses.TheAddressTableIsAsLongAsTheControlTable checks instead, in
 *  the one place both headers are visible. */
constexpr int numMixerAddresses = 7;
constexpr int numMasterAddresses = 5;
constexpr int numFilterAddresses = 3;

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
  /** The third per-channel value: how far a channel is spread into the 3D
   *  field. Core crossfades its stereo and multi encoders on it.
   *
   *  `3d` used to be a *toggle* there, which is why this address avoided the
   *  name for a while. Core's boolean has since moved to `4d` and `3d` takes
   *  the continuous value, so the name says what it does again. */
  juce::String channelThreeD{ "/channel/{ch}/3d" };

  /** The mixer's addresses, as a table over mixerControlOrder rather than as
   *  fifteen named fields.
   *
   *  Fifteen fields would be fifteen readAddress lines and fifteen JSON keys
   *  held in step by hand; the authority on what a channel strip has already
   *  exists (components/MixerControls.hh) and this follows it. Indexed the
   *  same way, so mixerChannel[i] is the address of mixerControlOrder[i].
   *
   *  Defaults are what A3 Core has always listened for — see
   *  web/a3-doc/src/ressources/osc.md. A typo does not fail loudly: the
   *  message is sent correctly, to an address nobody is subscribed to. */
  std::array<juce::String, numMixerAddresses> mixerChannel{
    "/channel/{ch}/gain",   "/channel/{ch}/eq/high",
    "/channel/{ch}/eq/mid", "/channel/{ch}/eq/low",
    "/channel/{ch}/volume", "/channel/{ch}/pfl",
    "/channel/{ch}/fx",
  };

  /** The summing section's addresses, not per channel. Same indexing rule as
   *  mixerChannel, over masterControlOrder. */
  std::array<juce::String, numMasterAddresses> mixerMaster{
    "/master/volume",        "/master/booth", "/master/phones_mix",
    "/master/phones_volume", "/master/return",
  };

  /** The one filter shared by all four channels. Same indexing rule as
   *  mixerChannel, over filterControlOrder. */
  std::array<juce::String, numFilterAddresses> mixerFilter{
    "/fx/mode",
    "/fx/frequency",
    "/fx/resonance",
  };

  // Outgoing, to an IEM plugin chain (SpatBackendIEM).
  juce::String iemAzimuth{ "/StereoEncoder/azimuth" };
  juce::String iemElevation{ "/StereoEncoder/elevation" };

  /** The one address this device sends in order to be *told* something.
   *
   *  A3 Core replays its whole state in answer: the lamps, and the position
   *  of every channel it has heard one for. Sent once at start-up, so the
   *  device adopts what is already sounding instead of asserting its own
   *  idea of it — the audible jump in
   *  issues/a3-motion-ui-total-recall-at-startup.md.
   *
   *  Has to match what Core listens for (OSC_ADDRESS_RECALL in a3-core.py).
   *  Nothing here can check that, which is the reason it is configurable. */
  juce::String stateRecall{ "/state/recall" };

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
