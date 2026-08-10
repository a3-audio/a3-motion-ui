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

#include <a3-motion-engine/MotionEngine.hh>

namespace a3
{

/** Parses/routes incoming OSC messages (VU meters, external beat clock) —
 *  extracted from A3MotionUIComponent::oscMessageReceived. Has no
 *  juce::Component dependency so it's directly unit-testable; visual
 *  side-effects are reported through Listener instead of being applied
 *  directly. */
class OscMessageHandler
{
public:
  struct Listener
  {
    virtual ~Listener () = default;

    // Channels 0-3: per-channel blob corona level.
    virtual void onChannelVU (int channel, float peak, float rms) = 0;
    // Channel 4: subwoofer -> sphere glow.
    virtual void onSubwooferVU (float peak, float rms) = 0;
    // Channels 5-8: speaker spotlights (speakerIndex = channel - 5).
    virtual void onSpeakerVU (int speakerIndex, float peak, float rms) = 0;

    // Always fires on /beat, for StatusBar's beat/BPM readout.
    virtual void onExternalBeatClock (int beat, int bar, float bpm) = 0;
    // Only fires on /beat while clockMode != 0 (EXT/PIO), for
    // LoopLengthDisplay's external-beat interpolation.
    virtual void onExternalBeatSync (int beat, int beatsPerBar) = 0;
  };

  OscMessageHandler (MotionEngine &engine, Listener &listener);

  void handleMessage (juce::OSCMessage const &message, int clockMode);

private:
  MotionEngine &_engine;
  Listener &_listener;
};

}
