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

#include "OscTruth.hh"

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
constexpr int numMixerAddresses = 8;
constexpr int numMasterAddresses = 5;
constexpr int numFilterAddresses = 3;

/** The OSC addresses this device speaks.
 *
 *  Ours come from the one truth, a3-core's a3-osc.json (oscAddressesFrom);
 *  none of them has a default here, because a default is a second truth. The
 *  IEM plug-ins' words are theirs and stay what the plug-ins speak. `{ch}`
 *  stands for the channel number and is substituted by withChannelIndex(). */
struct OscAddresses
{
  // Outgoing, to A3 Core (SpatBackendA3).
  juce::String channelAzimuth;
  juce::String channelElevation;
  /** The channel's own filter, which the encoder's two pots turn. Named
   *  pot_1 and pot_2 on the wire until 2026-09-30. */
  juce::String channelFilterFrequency;
  juce::String channelFilterQ;
  /** How far a channel is spread into the 3D field. Core crossfades its
   *  stereo and multi encoders on it. */
  juce::String channelThreeD;

  /** The mixer's addresses, as a table over mixerControlOrder rather than as
   *  fifteen named fields: the authority on what a channel strip has already
   *  exists (components/MixerControls.hh) and this follows it. Indexed the
   *  same way, so mixerChannel[i] is the address of mixerControlOrder[i]. */
  std::array<juce::String, numMixerAddresses> mixerChannel;

  /** The summing section's addresses, not per channel. Same indexing rule as
   *  mixerChannel, over masterControlOrder. */
  std::array<juce::String, numMasterAddresses> mixerMaster;

  /** The one filter shared by all four channels. Same indexing rule as
   *  mixerChannel, over filterControlOrder. */
  std::array<juce::String, numFilterAddresses> mixerFilter;

  // Outgoing, to an IEM plugin chain (SpatBackendIEM).
  juce::String iemAzimuth{ "/StereoEncoder/azimuth" };
  juce::String iemElevation{ "/StereoEncoder/elevation" };

  /** The one address this device sends in order to be *told* something:
   *  A3 Core replays its whole state in answer. Sent once at start-up, so
   *  the device adopts what is already sounding. */
  juce::String stateRecall;

  /** Motion naming itself to Core with the truth it speaks: at start, then
   *  every 30 s (DeviceHello.hh). */
  juce::String deviceHello;

  /** The beat clock going out -- sent every beat in INT mode. */
  juce::String beatOut;
  juce::String tap;
  juce::String clockMode;

  // Incoming. vuPrefix is matched with startsWith and the meter's number
  // read off what follows it.
  juce::String vuPrefix;
  juce::String energyRms{ "/EnergyVisualizer/RMS" };
  /** The beat clock coming in -- followed in EXT and PIO mode. The same
   *  address as beatOut; which one is in use follows the clock mode. */
  juce::String beatIn;
  /** StemDeck's preview of the music, relayed by Core (spec fpv-pilots,
   *  phase A). **Optional, and empty when the truth lacks it** -- the one
   *  exception to "no default": it is only listened for, never sent, and a
   *  truth from before it must stay usable (optionalOscAddressKeys). */
  juce::String stemdeckAhead;
};

/** Whether juce::OSCMessage will accept this as an address.
 *
 *  It has to be asked, because JUCE throws OSCFormatError on one it will
 *  not take. Mirrors JUCE's own rule for OSCAddressPattern (see
 *  juce_OSCAddress.cpp): not empty, a leading slash, and every
 *  '/'-separated token printable ASCII without a space or a '#'. */
bool isSendableOscAddress (juce::String const &address);

/** Substitutes `{ch}` with the channel's number on the wire: index 0 is
 *  channel 1. A pattern without the placeholder comes back unchanged. */
juce::String withChannelIndex (juce::String const &pattern, int index);

/** Every key of the truth's `addresses` Motion reads. */
juce::StringArray oscAddressKeys ();

/** Keys of the truth's `addresses` Motion reads if they are there. Not in
 *  oscAddressKeys(): a truth without them is not "missing" anything, so
 *  unusableOscTruth() and missingOscKeys() never name them. */
juce::StringArray optionalOscAddressKeys ();

/** Motion's addresses out of the truth. A key the truth lacks becomes
 *  "/a3-osc-missing/<key>": sendable, so nothing throws, and on the wire it
 *  says what is missing. */
OscAddresses oscAddressesFrom (OscTruth const &truth);

/** The keys of oscAddressKeys() the truth does not have. */
juce::StringArray missingOscKeys (OscTruth const &truth);

}
