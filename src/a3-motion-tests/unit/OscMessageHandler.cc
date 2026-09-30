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

#include <MadeUpOscTruth.hh>

#include <JuceHeader.h>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-ui/osc/OscMessageHandler.hh>
#include <a3-motion-ui/osc/VuRouting.hh>

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

  int outputVUCalls = 0;
  int lastOutputMeter = -1;

  void
  onOutputVU (int meter, float, float) override
  {
    ++outputVUCalls;
    lastOutputMeter = meter;
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


namespace
{
OscAddresses
madeUpAddresses ()
{
  return oscAddressesFrom (madeUpOscTruth ());
}
}

// The meters arrive by their number in the channel map; Motion shows them by
// what they measure. In madeUpOscTruth the map is scrambled: in2_pre is 6,
// main_sub 5, main_top2 7, main_top9 15, and 1 is a meter Motion never shows.
namespace
{
juce::OSCMessage
meter (int number, float peak = 0.5f, float rms = 0.25f)
{
  juce::OSCMessage message (madeUpAddresses ().vuPrefix + juce::String (number));
  message.addFloat32 (peak);
  message.addFloat32 (rms);
  return message;
}
}

TEST (OscMessageHandler, AnInputMeterGoesToItsChannel)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());
  handler.setVuRouting (vuRoutingFrom (madeUpOscTruth ()));

  handler.handleMessage (meter (6), /*clockMode=*/0); // in2_pre

  EXPECT_EQ (listener.channelVUCalls, 1);
  EXPECT_EQ (listener.lastChannel, 1);
  EXPECT_FLOAT_EQ (listener.lastPeak, 0.5f);
  EXPECT_FLOAT_EQ (listener.lastRms, 0.25f);
  EXPECT_EQ (listener.subwooferVUCalls, 0);
  EXPECT_EQ (listener.speakerVUCalls, 0);
  EXPECT_EQ (listener.outputVUCalls, 0);
}

// The main sub is two things at once: the sphere's glow, and the first meter
// of the master column.
TEST (OscMessageHandler, TheSubGlowsAndFillsTheFirstMasterMeter)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());
  handler.setVuRouting (vuRoutingFrom (madeUpOscTruth ()));

  handler.handleMessage (meter (5), /*clockMode=*/0); // main_sub

  EXPECT_EQ (listener.subwooferVUCalls, 1);
  EXPECT_EQ (listener.outputVUCalls, 1);
  EXPECT_EQ (listener.lastOutputMeter, 0);
  EXPECT_EQ (listener.channelVUCalls, 0);
}

// The first four tops light the towers, and all nine are in the column.
TEST (OscMessageHandler, ATopLightsItsTowerAndFillsItsMasterMeter)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());
  handler.setVuRouting (vuRoutingFrom (madeUpOscTruth ()));

  handler.handleMessage (meter (7), /*clockMode=*/0); // main_top2

  EXPECT_EQ (listener.speakerVUCalls, 1);
  EXPECT_EQ (listener.lastSpeakerIndex, 1);
  EXPECT_EQ (listener.outputVUCalls, 1);
  EXPECT_EQ (listener.lastOutputMeter, 2);
}

TEST (OscMessageHandler, AnUpperTopOnlyFillsTheMasterColumn)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());
  handler.setVuRouting (vuRoutingFrom (madeUpOscTruth ()));

  handler.handleMessage (meter (15), /*clockMode=*/0); // main_top9

  EXPECT_EQ (listener.outputVUCalls, 1);
  EXPECT_EQ (listener.lastOutputMeter, 9);
  EXPECT_EQ (listener.speakerVUCalls, 0);
}

TEST (OscMessageHandler, AMeterMotionDoesNotShowIsLeftAlone)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());
  handler.setVuRouting (vuRoutingFrom (madeUpOscTruth ()));

  handler.handleMessage (meter (1), /*clockMode=*/0); // "free"
  handler.handleMessage (meter (40), /*clockMode=*/0); // not in the map

  EXPECT_EQ (listener.channelVUCalls + listener.subwooferVUCalls
                 + listener.speakerVUCalls + listener.outputVUCalls,
             0);
}

// A meter the truth does not have is routed as 0 -- and a /vu/0 must not
// light every one of them.
TEST (OscMessageHandler, MetersTheTruthLacksAreNotAllFedByZero)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());
  handler.setVuRouting (vuRoutingFrom (parseOscTruth ("{}")));

  handler.handleMessage (meter (0), /*clockMode=*/0);

  EXPECT_EQ (listener.channelVUCalls + listener.subwooferVUCalls
                 + listener.speakerVUCalls + listener.outputVUCalls,
             0);
}

TEST (OscMessageHandler, BeatAlwaysNotifiesClockButOnlySyncsTempoInExternalMode)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());

  // Seed a known, non-zero tempo. Zero would make TempoClock's live clock
  // thread divide by zero when computing nanoseconds-per-tick, so never feed
  // 0 BPM to a running engine.
  engine.setTempoBPM (60.f);

  juce::OSCMessage message (madeUpAddresses ().beatIn);
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

// The beat-analyzer sends the tempo as a float, and it was read as an int:
// 117.454 became 117, and the clock ran 0.4 per cent slow against the beats it
// was being sent. Measured 2026-09-17, see
// issues/a3-motion-ui-haelt-den-takt-nicht.md.
TEST (OscMessageHandler, TheTempoArrivesWithItsFraction)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());
  engine.setTempoBPM (60.f);

  juce::OSCMessage message (madeUpAddresses ().beatIn);
  message.addInt32 (1);
  message.addInt32 (3);
  message.addFloat32 (117.454f);

  handler.handleMessage (message, /*clockMode=*/1);

  EXPECT_FLOAT_EQ (engine.getTempoBPM (), 117.454f);
  EXPECT_FLOAT_EQ (listener.lastBpm, 117.454f);
}

// The IEM EnergyVisualizer sends one float per grid point in a single message.
// Order is the plugin's, so the handler must pass it through untouched.
TEST (OscMessageHandler, EnergyGridIsForwardedInOrder)
{
  HeightMapSphere heightMap;
  MotionEngine engine{ 4, heightMap };
  RecordingListener listener;
  OscMessageHandler handler{ engine, listener };
  handler.setAddresses (madeUpAddresses ());

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
  handler.setAddresses (madeUpAddresses ());

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
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().channelAzimuth, 1));
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
  handler.setAddresses (madeUpAddresses ());

  // Degrees, and negative ones at that: Core clamps elevation to -90..90 and
  // the OSC reference's "[0-1]" for this address is wrong. A handler that
  // assumed a normalised value would put every sound below the horizon on
  // the horizon.
  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().channelElevation, 3));
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
  handler.setAddresses (madeUpAddresses ());

  handler.handleMessage (juce::OSCMessage (withChannelIndex (madeUpAddresses ().channelAzimuth, 1)),
                         /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 0);
}

TEST (OscMessageHandler, IgnoresAPositionThatIsNotANumber)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  RecordingListener listener;
  OscMessageHandler handler (engine, listener);
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().channelAzimuth, 1));
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
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().channelAzimuth, 9));
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
  handler.setAddresses (madeUpAddresses ());

  handler.setVuRouting (vuRoutingFrom (madeUpOscTruth ()));

  handler.handleMessage (meter (4), /*clockMode=*/0); // in1_pre

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
  handler.setAddresses (madeUpAddresses ());

  auto addresses = madeUpAddresses ();
  auto const before = addresses.channelAzimuth;
  addresses.channelAzimuth = "/a3/{ch}/az";
  handler.setAddresses (addresses);

  // Channel 2 on the wire is the second channel, index 1.
  juce::OSCMessage message ("/a3/2/az");
  message.addFloat32 (-12.f);

  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.azimuthCalls, 1);
  EXPECT_EQ (listener.lastPositionChannel, 1);
  EXPECT_FLOAT_EQ (listener.lastAzimuth, -12.f);

  // And the old address is no longer one.
  juce::OSCMessage stale (withChannelIndex (before, 1));
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
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage first (withChannelIndex (madeUpAddresses ().channelFilterFrequency, 2));
  first.addFloat32 (0.4f);
  handler.handleMessage (first, /*clockMode=*/0);

  EXPECT_EQ (listener.valueCalls, 1);
  EXPECT_EQ (listener.lastPositionChannel, 2);
  EXPECT_EQ (listener.lastWhich, Value::Pot1);
  EXPECT_FLOAT_EQ (listener.lastValue, 0.4f);

  juce::OSCMessage second (withChannelIndex (madeUpAddresses ().channelFilterQ, 2));
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
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().channelThreeD, 1));
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
  handler.setAddresses (madeUpAddresses ());

  std::vector<std::pair<juce::String, Value> > const expected{
    { withChannelIndex (madeUpAddresses ().channelAzimuth, 0), Value::Azimuth },
    { withChannelIndex (madeUpAddresses ().channelElevation, 0), Value::Elevation },
    { withChannelIndex (madeUpAddresses ().channelFilterFrequency, 0), Value::Pot1 },
    { withChannelIndex (madeUpAddresses ().channelFilterQ, 0), Value::Pot2 },
    { withChannelIndex (madeUpAddresses ().channelThreeD, 0), Value::ThreeD },
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
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().mixerChannel[0], 2));
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
  handler.setAddresses (madeUpAddresses ());

  std::vector<juce::String> const addresses{
    withChannelIndex (madeUpAddresses ().mixerChannel[0], 0),   withChannelIndex (madeUpAddresses ().mixerChannel[1], 0),
    withChannelIndex (madeUpAddresses ().mixerChannel[2], 0), withChannelIndex (madeUpAddresses ().mixerChannel[3], 0),
    withChannelIndex (madeUpAddresses ().mixerChannel[4], 0), withChannelIndex (madeUpAddresses ().mixerChannel[5], 0),
    withChannelIndex (madeUpAddresses ().mixerChannel[6], 0),    withChannelIndex (madeUpAddresses ().mixerChannel[7], 0),
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
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().channelFilterFrequency, 3));
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
  handler.setAddresses (madeUpAddresses ());

  std::vector<juce::String> const addresses{
    madeUpAddresses ().mixerMaster[0],        madeUpAddresses ().mixerMaster[1], madeUpAddresses ().mixerMaster[2],
    madeUpAddresses ().mixerMaster[3], madeUpAddresses ().mixerMaster[4],
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
  handler.setAddresses (madeUpAddresses ());

  // /fx/mode arrives as a number, not as the word the desk's LEDs get: 1 is
  // high pass. That is the spelling this device already *sends* on the same
  // address, which is the whole reason Core answers on it.
  std::vector<juce::String> const addresses{
    madeUpAddresses ().mixerFilter[0],
    madeUpAddresses ().mixerFilter[1],
    madeUpAddresses ().mixerFilter[2],
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
  handler.setAddresses (madeUpAddresses ());

  juce::OSCMessage message (withChannelIndex (madeUpAddresses ().mixerChannel[4], 0));
  message.addFloat32 (0.8f);
  handler.handleMessage (message, /*clockMode=*/0);

  EXPECT_EQ (listener.mixerValueCalls, 1);
  EXPECT_EQ (listener.masterValueCalls, 0);
  EXPECT_EQ (listener.filterValueCalls, 0);
}
