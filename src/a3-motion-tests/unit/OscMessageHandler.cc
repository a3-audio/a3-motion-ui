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

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-ui/osc/OscMessageHandler.hh>

using namespace a3;

namespace
{

struct RecordingListener : public OscMessageHandler::Listener
{
  int channelVUCalls = 0;
  int lastChannel = -1;
  float lastPeak = 0.f;
  float lastRms = 0.f;

  int subwooferVUCalls = 0;
  int speakerVUCalls = 0;
  int lastSpeakerIndex = -1;

  int externalBeatClockCalls = 0;
  int lastBeat = -1;
  int lastBar = -1;
  float lastBpm = 0.f;

  int externalBeatSyncCalls = 0;

  int valueCalls = 0;
  int azimuthCalls = 0;
  int elevationCalls = 0;
  int lastPositionChannel = -1;
  float lastAzimuth = 0.f;
  float lastElevation = 0.f;
  float lastValue = 0.f;
  OscMessageHandler::Listener::ChannelValue lastWhich
      = OscMessageHandler::Listener::ChannelValue::Azimuth;

  int energyGridCalls = 0;
  int lastEnergyCount = -1;
  float lastEnergyFirst = -1.f;
  float lastEnergyLast = -1.f;

  int mixerValueCalls = 0;
  int lastMixerChannel = -1;
  int lastMixerSlot = -1;
  float lastMixerValue = -1.f;

  int masterValueCalls = 0;
  int lastMasterSlot = -1;
  float lastMasterValue = -1.f;

  int filterValueCalls = 0;
  int lastFilterSlot = -1;
  float lastFilterValue = -1.f;

  void
  onMixerChannelValue (int channel, int slot, float value) override
  {
    ++mixerValueCalls;
    lastMixerChannel = channel;
    lastMixerSlot = slot;
    lastMixerValue = value;
  }

  void
  onMasterValue (int slot, float value) override
  {
    ++masterValueCalls;
    lastMasterSlot = slot;
    lastMasterValue = value;
  }

  void
  onFilterValue (int slot, float value) override
  {
    ++filterValueCalls;
    lastFilterSlot = slot;
    lastFilterValue = value;
  }

  void
  onEnergyGrid (float const *values, int count) override
  {
    ++energyGridCalls;
    lastEnergyCount = count;
    if (count > 0)
      {
        lastEnergyFirst = values[0];
        lastEnergyLast = values[count - 1];
      }
  }

  void
  onChannelVU (int channel, float peak, float rms) override
  {
    ++channelVUCalls;
    lastChannel = channel;
    lastPeak = peak;
    lastRms = rms;
  }

  void
  onSubwooferVU (float, float) override
  {
    ++subwooferVUCalls;
  }

  void
  onSpeakerVU (int speakerIndex, float, float) override
  {
    ++speakerVUCalls;
    lastSpeakerIndex = speakerIndex;
  }

  void
  onExternalBeatClock (int beat, int bar, float bpm) override
  {
    ++externalBeatClockCalls;
    lastBeat = beat;
    lastBar = bar;
    lastBpm = bpm;
  }

  void
  onExternalBeatSync (int, int) override
  {
    ++externalBeatSyncCalls;
  }

  void
  onChannelValue (int channel, OscMessageHandler::Listener::ChannelValue which,
                  float value) override
  {
    using Value = OscMessageHandler::Listener::ChannelValue;
    ++valueCalls;
    lastPositionChannel = channel;
    lastWhich = which;
    lastValue = value;

    if (which == Value::Azimuth)
      {
        ++azimuthCalls;
        lastAzimuth = value;
      }
    else if (which == Value::Elevation)
      {
        ++elevationCalls;
        lastElevation = value;
      }
  }
};

TEST (OscMessageHandler, RoutesChannelVUToListener)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/vu/2");
  message.addFloat32 (0.5f);
  message.addFloat32 (0.25f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.channelVUCalls, 1);
  EXPECT_EQ (listener.lastChannel, 2);
  EXPECT_FLOAT_EQ (listener.lastPeak, 0.5f);
  EXPECT_FLOAT_EQ (listener.lastRms, 0.25f);
  EXPECT_EQ (listener.subwooferVUCalls, 0);
  EXPECT_EQ (listener.speakerVUCalls, 0);
}

TEST (OscMessageHandler, RoutesSubwooferVUToListener)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/vu/4");
  message.addFloat32 (0.9f);
  message.addFloat32 (0.8f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.subwooferVUCalls, 1);
  EXPECT_EQ (listener.channelVUCalls, 0);
}

TEST (OscMessageHandler, RoutesSpeakerVUToListenerWithZeroBasedIndex)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/vu/6"); // speaker index 6 - 5 = 1
  message.addFloat32 (0.1f);
  message.addFloat32 (0.2f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.speakerVUCalls, 1);
  EXPECT_EQ (listener.lastSpeakerIndex, 1);
}

TEST (OscMessageHandler, BeatAlwaysNotifiesClockButOnlySyncsTempoInExternalMode)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  // Seed a known, non-zero tempo. Zero would make TempoClock's live clock
  // thread divide by zero when computing nanoseconds-per-tick, so never feed
  // 0 BPM to a running engine.
  engine.setTempoBPM (60.f);

  juce::OSCMessage message ("/beat");
  message.addInt32 (2);   // beat
  message.addInt32 (5);   // bar
  message.addInt32 (128); // bpm

  // clockMode == 0 (INT): StatusBar-style notification happens, but the
  // engine's tempo is NOT overwritten and LoopLengthDisplay is not synced.
  handler.handleMessage (message, /*clockMode=*/0);
  EXPECT_EQ (listener.externalBeatClockCalls, 1);
  EXPECT_EQ (listener.lastBeat, 2);
  EXPECT_EQ (listener.lastBar, 5);
  EXPECT_FLOAT_EQ (listener.lastBpm, 128.f);
  EXPECT_EQ (listener.externalBeatSyncCalls, 0);
  EXPECT_FLOAT_EQ (engine.getTempoBPM (), 60.f);

  // clockMode != 0 (EXT/PIO): tempo is synced and the sync listener fires.
  handler.handleMessage (message, /*clockMode=*/1);
  EXPECT_EQ (listener.externalBeatClockCalls, 2);
  EXPECT_EQ (listener.externalBeatSyncCalls, 1);
  EXPECT_FLOAT_EQ (engine.getTempoBPM (), 128.f);
}

// The IEM EnergyVisualizer sends one float per grid point in a single message.
// Order is the plugin's, so the handler must pass it through untouched.
TEST (OscMessageHandler, EnergyGridIsForwardedInOrder)
{
  HeightMapSphere heightMap;
  MotionEngine engine{ 4, heightMap };
  RecordingListener listener;
  OscMessageHandler handler{ engine, listener };

  juce::OSCMessage message{ juce::OSCAddressPattern ("/EnergyVisualizer/RMS") };
  for (int i = 0; i < 426; ++i)
    message.addFloat32 (static_cast<float> (i) * 0.001f);

  handler.handleMessage (message, 0);

  EXPECT_EQ (listener.energyGridCalls, 1);
  EXPECT_EQ (listener.lastEnergyCount, 426);
  EXPECT_FLOAT_EQ (listener.lastEnergyFirst, 0.f);
  EXPECT_FLOAT_EQ (listener.lastEnergyLast, 0.425f);
}

// A short message would leave the tail of the map holding stale values, which
// reads as energy that is no longer there.
TEST (OscMessageHandler, EnergyGridOfTheWrongLengthIsRejected)
{
  HeightMapSphere heightMap;
  MotionEngine engine{ 4, heightMap };
  RecordingListener listener;
  OscMessageHandler handler{ engine, listener };

  juce::OSCMessage message{ juce::OSCAddressPattern ("/EnergyVisualizer/RMS") };
  for (int i = 0; i < 12; ++i)
    message.addFloat32 (0.5f);

  handler.handleMessage (message, 0);

  EXPECT_EQ (listener.energyGridCalls, 0);
}

}

// A3 Core answers /state/recall with the position of every channel it has
// heard one for. It is the only device that can: it writes the position
// straight to the IEM plugins' own OSC port rather than through a REAPER
// track, so nothing on the rig reports it back. Until these arrived, the
// answer landed in this device's socket and was dropped -- see
// issues/a3-motion-ui-nimmt-die-position-nicht-entgegen.md.

TEST (OscMessageHandler, RoutesChannelAzimuthToListener)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/channel/1/azimuth");
  message.addFloat32 (45.f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 1);
  EXPECT_EQ (listener.lastPositionChannel, 1);
  EXPECT_FLOAT_EQ (listener.lastAzimuth, 45.f);
  EXPECT_EQ (listener.elevationCalls, 0);
}

TEST (OscMessageHandler, RoutesChannelElevationToListener)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  // Degrees, and negative ones at that: Core clamps elevation to -90..90 and
  // the OSC reference's "[0-1]" for this address is wrong. A handler that
  // assumed a normalised value would put every sound below the horizon on
  // the horizon.
  juce::OSCMessage message ("/channel/3/elevation");
  message.addFloat32 (-63.f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.elevationCalls, 1);
  EXPECT_EQ (listener.lastPositionChannel, 3);
  EXPECT_FLOAT_EQ (listener.lastElevation, -63.f);
  EXPECT_EQ (listener.azimuthCalls, 0);
}

TEST (OscMessageHandler, IgnoresAPositionWithNoValue)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  handler.handleMessage (juce::OSCMessage ("/channel/1/azimuth"),
                         /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 0);
}

TEST (OscMessageHandler, IgnoresAPositionThatIsNotANumber)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/channel/1/azimuth");
  message.addString ("nach vorne");

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 0);
}

TEST (OscMessageHandler, IgnoresAChannelThisRigDoesNotHave)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/channel/9/azimuth");
  message.addFloat32 (45.f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 0);
}

TEST (OscMessageHandler, AVuMessageIsStillNotAPosition)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/vu/2");
  message.addFloat32 (0.5f);
  message.addFloat32 (0.25f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.channelVUCalls, 1);
  EXPECT_EQ (listener.azimuthCalls, 0);
  EXPECT_EQ (listener.elevationCalls, 0);
}

TEST (OscMessageHandler, FollowsAReconfiguredPositionAddress)
{
  // The addresses are configurable, so the incoming address cannot be
  // matched by a hardcoded prefix -- it has to be built from the same table
  // the sender would use.
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  OscAddresses addresses;
  addresses.channelAzimuth = "/a3/{ch}/az";
  handler.setAddresses (addresses);

  juce::OSCMessage message ("/a3/2/az");
  message.addFloat32 (-12.f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 1);
  EXPECT_EQ (listener.lastPositionChannel, 2);
  EXPECT_FLOAT_EQ (listener.lastAzimuth, -12.f);

  // And the old address is no longer one.
  juce::OSCMessage stale ("/channel/2/azimuth");
  stale.addFloat32 (99.f);
  handler.handleMessage (stale, /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 1);
}

// The other three per-channel values, added 2026-09-12 once A3 Core could
// answer for them: the two encoder pots come back through the reverse table
// (a plain linear map, not a curve), the crossfade out of Core's own memory
// because REAPER holds two gains and cannot be asked which input made them.

TEST (OscMessageHandler, RoutesTheEncoderPotsToListener)
{
  using Value = OscMessageHandler::Listener::ChannelValue;

  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage first ("/channel/2/pot_1");
  first.addFloat32 (0.4f);
  handler.handleMessage (first, /*clockMode=*/0);

  EXPECT_EQ (listener.valueCalls, 1);
  EXPECT_EQ (listener.lastPositionChannel, 2);
  EXPECT_EQ (listener.lastWhich, Value::Pot1);
  EXPECT_FLOAT_EQ (listener.lastValue, 0.4f);

  juce::OSCMessage second ("/channel/2/pot_2");
  second.addFloat32 (0.5f);
  handler.handleMessage (second, /*clockMode=*/0);

  EXPECT_EQ (listener.valueCalls, 2);
  EXPECT_EQ (listener.lastWhich, Value::Pot2);
  EXPECT_FLOAT_EQ (listener.lastValue, 0.5f);
}

TEST (OscMessageHandler, RoutesTheCrossfadeToListener)
{
  using Value = OscMessageHandler::Listener::ChannelValue;

  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/channel/1/3d");
  message.addFloat32 (0.62f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.valueCalls, 1);
  EXPECT_EQ (listener.lastPositionChannel, 1);
  EXPECT_EQ (listener.lastWhich, Value::ThreeD);
  EXPECT_FLOAT_EQ (listener.lastValue, 0.62f);
}

TEST (OscMessageHandler, TheFiveValuesAreToldApart)
{
  // One call with a tag is only an improvement on five callbacks if the tag
  // is right; a table is exactly the shape that quietly pairs the wrong two.
  using Value = OscMessageHandler::Listener::ChannelValue;

  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  std::vector<std::pair<juce::String, Value> > const expected{
    { "/channel/0/azimuth", Value::Azimuth },
    { "/channel/0/elevation", Value::Elevation },
    { "/channel/0/pot_1", Value::Pot1 },
    { "/channel/0/pot_2", Value::Pot2 },
    { "/channel/0/3d", Value::ThreeD },
  };

  for (auto const &[address, which] : expected)
    {
      juce::OSCMessage message (address);
      message.addFloat32 (0.5f);
      handler.handleMessage (message, /*clockMode=*/0);
      EXPECT_EQ (listener.lastWhich, which) << address;
    }

  EXPECT_EQ (listener.valueCalls, static_cast<int> (expected.size ()));
}

// ── What REAPER says about the channel strip ────────────────────────────────

TEST (OscMessageHandler, RoutesAMixerChannelValueToItsSlot)
{
  // A3 Core relays gain, the three bands and volume back from REAPER as of
  // 2026-09-12. Before that the strip came up at its own defaults and stayed
  // there -- GAIN and VOL reading zero on a rig that was making sound.
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/channel/2/gain");
  message.addFloat32 (0.4f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.mixerValueCalls, 1);
  EXPECT_EQ (listener.lastMixerChannel, 2);
  EXPECT_EQ (listener.lastMixerSlot, 0); // gain is first in mixerControlOrder
  EXPECT_FLOAT_EQ (listener.lastMixerValue, 0.4f);

  // And it is not mistaken for one of the five position-and-pot values.
  EXPECT_EQ (listener.valueCalls, 0);
}

TEST (OscMessageHandler, EveryStripAddressFindsItsOwnSlot)
{
  // The slot is an index into OscAddresses::mixerChannel, which is indexed
  // the same way as MixerControls.hh's mixerControlOrder -- a pairing no
  // compiler checks and the exact shape that goes wrong silently. Eight
  // addresses, eight slots, in order.
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  std::vector<juce::String> const addresses{
    "/channel/0/gain",   "/channel/0/eq/high",
    "/channel/0/eq/mid", "/channel/0/eq/low",
    "/channel/0/volume", "/channel/0/fx-send",
    "/channel/0/pfl",    "/channel/0/fx",
  };

  for (std::size_t slot = 0; slot < addresses.size (); ++slot)
    {
      juce::OSCMessage message (addresses[slot]);
      message.addFloat32 (0.5f);
      handler.handleMessage (message, /*clockMode=*/0);
      EXPECT_EQ (listener.lastMixerSlot, static_cast<int> (slot))
          << addresses[slot];
    }

  EXPECT_EQ (listener.mixerValueCalls, static_cast<int> (addresses.size ()));
}

TEST (OscMessageHandler, TheStripDoesNotSwallowThePots)
{
  // /channel/n/pot_1 and /channel/n/gain are both per-channel floats and the
  // two tables are walked one after the other. A strip address that matched
  // a pot -- or the other way round -- would put a filter frequency on a
  // gain knob, and nothing about either value would look wrong.
  using Value = OscMessageHandler::Listener::ChannelValue;

  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/channel/3/pot_1");
  message.addFloat32 (0.3f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.valueCalls, 1);
  EXPECT_EQ (listener.lastWhich, Value::Pot1);
  EXPECT_EQ (listener.mixerValueCalls, 0);
}

// ── The master section and the shared filter ────────────────────────────────

TEST (OscMessageHandler, EveryMasterAddressFindsItsOwnSlot)
{
  // None of these belongs to a channel, which is why A3 Core had no way back
  // for them at all until 2026-09-12 -- and why this page came up at its own
  // defaults for as long as it existed.
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  std::vector<juce::String> const addresses{
    "/master/volume",        "/master/booth", "/master/phones_mix",
    "/master/phones_volume", "/master/return",
  };

  for (std::size_t slot = 0; slot < addresses.size (); ++slot)
    {
      juce::OSCMessage message (addresses[slot]);
      message.addFloat32 (0.25f);
      handler.handleMessage (message, /*clockMode=*/0);
      EXPECT_EQ (listener.lastMasterSlot, static_cast<int> (slot))
          << addresses[slot];
    }

  EXPECT_EQ (listener.masterValueCalls, static_cast<int> (addresses.size ()));
  EXPECT_EQ (listener.filterValueCalls, 0);
  EXPECT_EQ (listener.mixerValueCalls, 0);
}

TEST (OscMessageHandler, EveryFilterAddressFindsItsOwnSlot)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  // /fx/mode arrives as a number, not as the word the desk's LEDs get: 1 is
  // high pass. That is the spelling this device already *sends* on the same
  // address, which is the whole reason Core answers on it.
  std::vector<juce::String> const addresses{
    "/fx/mode",
    "/fx/frequency",
    "/fx/resonance",
  };

  for (std::size_t slot = 0; slot < addresses.size (); ++slot)
    {
      juce::OSCMessage message (addresses[slot]);
      message.addFloat32 (1.f);
      handler.handleMessage (message, /*clockMode=*/0);
      EXPECT_EQ (listener.lastFilterSlot, static_cast<int> (slot))
          << addresses[slot];
    }

  EXPECT_EQ (listener.filterValueCalls, static_cast<int> (addresses.size ()));
  EXPECT_EQ (listener.masterValueCalls, 0);
}

TEST (OscMessageHandler, AChannelAddressIsNotAMasterOne)
{
  // /master/volume and /channel/0/volume are one word apart and the two
  // tables are walked one after the other. Crossing them would put the room's
  // level on a channel fader, and neither number would look wrong.
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);

  juce::OSCMessage message ("/channel/0/volume");
  message.addFloat32 (0.8f);
  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.mixerValueCalls, 1);
  EXPECT_EQ (listener.masterValueCalls, 0);
  EXPECT_EQ (listener.filterValueCalls, 0);
}
