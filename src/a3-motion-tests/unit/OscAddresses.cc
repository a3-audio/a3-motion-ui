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

#include <a3-motion-engine/OscAddresses.hh>
#include <a3-motion-engine/OscTruth.hh>

#include <a3-motion-ui/components/MixerControls.hh>

#include <MadeUpOscTruth.hh>

using namespace a3;


TEST (OscAddresses, EveryAddressOfOursComesFromTheTruth)
{
  auto const a = oscAddressesFrom (madeUpOscTruth ());

  EXPECT_EQ (a.channelAzimuth, "/t/channel.azimuth/{ch}");
  EXPECT_EQ (a.channelElevation, "/t/channel.elevation/{ch}");
  EXPECT_EQ (a.channelFilterFrequency, "/t/channel.filter.frequency/{ch}");
  EXPECT_EQ (a.channelFilterQ, "/t/channel.filter.q/{ch}");
  EXPECT_EQ (a.channelThreeD, "/t/channel.3d/{ch}");
  EXPECT_EQ (a.stateRecall, "/t/state.recall");
  EXPECT_EQ (a.beatOut, "/t/beat");
  EXPECT_EQ (a.beatIn, "/t/beat");
  EXPECT_EQ (a.tap, "/t/tap");
  EXPECT_EQ (a.clockMode, "/t/clockmode");
}

TEST (OscAddresses, TheMixerTablesComeFromTheTruthInControlOrder)
{
  auto const a = oscAddressesFrom (madeUpOscTruth ());
  auto const channel = [&a] (MixerControl control) {
    return a.mixerChannel[static_cast<std::size_t> (controlSlot (control))];
  };
  auto const master = [&a] (MasterControl control) {
    return a.mixerMaster[static_cast<std::size_t> (controlSlot (control))];
  };
  auto const filter = [&a] (FilterControl control) {
    return a.mixerFilter[static_cast<std::size_t> (controlSlot (control))];
  };

  EXPECT_EQ (channel (MixerControl::Gain), "/t/channel.gain/{ch}");
  EXPECT_EQ (channel (MixerControl::EqHigh), "/t/channel.eq.high/{ch}");
  EXPECT_EQ (channel (MixerControl::EqMid), "/t/channel.eq.mid/{ch}");
  EXPECT_EQ (channel (MixerControl::EqLow), "/t/channel.eq.low/{ch}");
  EXPECT_EQ (channel (MixerControl::Volume), "/t/channel.volume/{ch}");
  EXPECT_EQ (channel (MixerControl::AuxSend), "/t/channel.aux-send/{ch}");
  EXPECT_EQ (channel (MixerControl::Cue), "/t/channel.cue/{ch}");
  EXPECT_EQ (channel (MixerControl::Fx), "/t/channel.filter/{ch}");

  EXPECT_EQ (master (MasterControl::Volume), "/t/master.volume");
  EXPECT_EQ (master (MasterControl::Booth), "/t/master.booth");
  EXPECT_EQ (master (MasterControl::PhonesMix), "/t/master.phones-mix");
  EXPECT_EQ (master (MasterControl::PhonesVolume), "/t/master.phones-volume");
  EXPECT_EQ (master (MasterControl::Return), "/t/master.aux-return");

  EXPECT_EQ (filter (FilterControl::Mode), "/t/filter.mode");
  EXPECT_EQ (filter (FilterControl::Frequency), "/t/filter.frequency");
  EXPECT_EQ (filter (FilterControl::Resonance), "/t/filter.resonance");
}

// The meters arrive as /vu/<n>; the handler matches the part before the
// number and reads the number off what follows.
TEST (OscAddresses, TheVuPrefixIsThePatternUpToItsNumber)
{
  EXPECT_EQ (oscAddressesFrom (madeUpOscTruth ()).vuPrefix, "/t/vu/");
}

// The IEM plug-ins' words are theirs, not ours: the truth lists them under
// `external` and they stay what the plug-ins speak.
TEST (OscAddresses, ThePluginsKeepTheirOwnWords)
{
  auto const a = oscAddressesFrom (madeUpOscTruth ());
  EXPECT_EQ (a.iemAzimuth, "/StereoEncoder/azimuth");
  EXPECT_EQ (a.iemElevation, "/StereoEncoder/elevation");
  EXPECT_EQ (a.energyRms, "/EnergyVisualizer/RMS");
}

// Nothing is invented for a key the truth lacks -- but nothing may crash
// either: JUCE throws on an address it will not take. The mark is sendable,
// and on the wire it names what is missing.
TEST (OscAddresses, AKeyTheTruthLacksIsMarkedNotInvented)
{
  auto const truth = madeUpOscTruth ({ "channel.volume", "tap" });
  auto const a = oscAddressesFrom (truth);

  auto const volume = a.mixerChannel[static_cast<std::size_t> (
      controlSlot (MixerControl::Volume))];
  EXPECT_EQ (volume, "/a3-osc-missing/channel.volume");
  EXPECT_TRUE (isSendableOscAddress (volume));
  EXPECT_EQ (a.tap, "/a3-osc-missing/tap");

  auto const missing = missingOscKeys (truth);
  EXPECT_TRUE (missing.contains ("channel.volume"));
  EXPECT_TRUE (missing.contains ("tap"));
  EXPECT_EQ (missing.size (), 2);
}

TEST (OscAddresses, AnInvalidTruthLacksEverything)
{
  auto const truth = parseOscTruth ("{ not json");
  EXPECT_EQ (missingOscKeys (truth).size (), oscAddressKeys ().size ());
  EXPECT_TRUE (isSendableOscAddress (oscAddressesFrom (truth).beatOut));
}

// Channels count from 1 on the wire since 2026-09-30, and from 0 everywhere
// in here. The one function that crosses says so in its name.
TEST (OscAddresses, AChannelIndexGoesOutAsItsNumber)
{
  EXPECT_EQ (withChannelIndex ("/channel/{ch}/azimuth", 0),
             "/channel/1/azimuth");
  EXPECT_EQ (withChannelIndex ("/channel/{ch}/azimuth", 3),
             "/channel/4/azimuth");
  EXPECT_EQ (withChannelIndex ("/src/{ch}/{ch}", 6), "/src/7/7");
}

TEST (OscAddresses, APatternWithoutThePlaceholderIsLeftAlone)
{
  EXPECT_EQ (withChannelIndex ("/StereoEncoder/azimuth", 3),
             "/StereoEncoder/azimuth");
}

TEST (OscAddresses, JuceAcceptsEveryAddressFromTheTruth)
{
  auto const a = oscAddressesFrom (madeUpOscTruth ());

  for (auto const &pattern :
       { a.channelAzimuth, a.channelElevation, a.channelFilterFrequency,
         a.channelFilterQ, a.channelThreeD, a.iemAzimuth, a.iemElevation,
         a.beatOut, a.beatIn, a.tap, a.clockMode, a.stateRecall })
    for (int index = 0; index < 4; ++index)
      EXPECT_TRUE (isSendableOscAddress (withChannelIndex (pattern, index)))
          << pattern;
}

// The engine holds the addresses and the ui holds the order they are in, and
// the two never meet in a translation unit -- the engine deliberately does not
// include a ui header. This is the one place both are visible, so it is the
// only place the agreement can be checked at all.
TEST (OscAddresses, TheAddressTableIsAsLongAsTheControlTable)
{
  EXPECT_EQ (numMixerAddresses, numMixerControls);
  EXPECT_EQ (numMasterAddresses, numMasterControls);
  EXPECT_EQ (numFilterAddresses, numFilterControls);
}
