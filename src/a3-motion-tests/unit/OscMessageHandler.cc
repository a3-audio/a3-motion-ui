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

}
