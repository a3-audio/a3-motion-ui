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
#include <a3-motion-engine/OscAddresses.hh>

namespace a3
{

/** Parses/routes incoming OSC messages (VU meters, external beat clock) —
 *  extracted from A3MotionUIComponent::oscMessageReceived. Has no
 *  juce::Component dependency so it's directly unit-testable; visual
 *  side-effects are reported through Listener instead of being applied
 *  directly. */
/** Grid points the IEM EnergyVisualizer sends per message. */
constexpr int energyGridPointCount = 426;

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

    // /EnergyVisualizer/RMS: one value per grid point of the IEM plugin's
    // 426-point sphere, in the plugin's own order. The pointer is only valid
    // for the duration of the call.
    virtual void onEnergyGrid (float const *values, int count) = 0;

    // Always fires on /beat, for StatusBar's beat/BPM readout.
    virtual void onExternalBeatClock (int beat, int bar, float bpm) = 0;
    // Only fires on /beat while clockMode != 0 (EXT/PIO), for
    // LoopLengthDisplay's external-beat interpolation.
    virtual void onExternalBeatSync (int beat, int beatsPerBar) = 0;

    /** One per-channel value coming back from A3 Core -- in practice, the
     *  answer to /state/recall at start-up.
     *
     *  One call with a tag rather than five of the same shape. They differ
     *  only in which address carried them and which setter takes them, and
     *  five near-identical virtuals is five places for the sixth to be
     *  forgotten.
     *
     *  Units are the wire's, not normalised: azimuth -180..180 and elevation
     *  -90..90 in **degrees**; the two pots and the crossfade 0..1. (The OSC
     *  reference gives elevation as [0-1]; it is wrong, see
     *  issues/a3-core-position-hat-keinen-rueckweg-und-keinen-halter.md.)
     *
     *  Each value arrives as its own message -- there is no moment at which
     *  two of them are known together.
     *
     *  What to do with one is the listener's call, not this class's: whether
     *  a channel is playing a trajectory is state this parser has no
     *  business knowing. */
    enum class ChannelValue
    {
      Azimuth,
      Elevation,
      Pot1,
      Pot2,
      ThreeD,
    };

    virtual void onChannelValue (int channel, ChannelValue which,
                                 float value)
        = 0;

    /** One of the channel strip's controls, as A3 Core relays it back from
     *  REAPER.
     *
     *  `slot` is an index into OscAddresses::mixerChannel, which is indexed
     *  the same way as MixerControls.hh's mixerControlOrder -- so the caller
     *  turns it into a MixerControl with one array lookup. An index rather
     *  than the enum because that pairing is already the seam between the
     *  engine's address table and the ui's control list, and this file has no
     *  business learning a second vocabulary to cross it.
     *
     *  Separate from onChannelValue and its five-value tag on purpose. Those
     *  five are the room -- where a sound is, how wide, how filtered -- and
     *  three of them are things an action script drives. These eight are a
     *  mixing desk. One enum over thirteen values would put a gain and an
     *  azimuth in the same switch, and the first reader to add a case would
     *  have to work out which half they were in. */
    virtual void onMixerChannelValue (int channel, int slot, float value) = 0;
  };

  OscMessageHandler (MotionEngine &engine, Listener &listener);

  /** New addresses, e.g. after config.json was edited on the device.
   *  Messages arrive through OSCReceiver::MessageLoopCallback, so this and
   *  handleMessage() are both on the message thread — no handover needed. */
  void setAddresses (OscAddresses const &addresses);

  void handleMessage (juce::OSCMessage const &message, int clockMode);

private:
  OscAddresses _addresses;
  MotionEngine &_engine;
  Listener &_listener;
};

}
