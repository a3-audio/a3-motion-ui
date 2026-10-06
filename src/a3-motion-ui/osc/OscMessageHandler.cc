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

#include "OscMessageHandler.hh"

#include <a3-motion-engine/tempo/BeatTrace.hh>

#include <algorithm>
#include <array>

namespace a3
{

OscMessageHandler::OscMessageHandler (MotionEngine &engine, Listener &listener)
    : _addresses (oscAddressesFrom (installedOscTruth ())),
      _vuRouting (vuRoutingFrom (installedOscTruth ())), _engine (engine),
      _listener (listener)
{
}

void
OscMessageHandler::setVuRouting (VuRouting const &routing)
{
  _vuRouting = routing;
}

void
OscMessageHandler::setAddresses (OscAddresses const &addresses)
{
  _addresses = addresses;
}

void
OscMessageHandler::routeMeter (int number, float peak, float rms)
{
  // A meter the truth lacks is routed as 0; no message may reach it.
  if (number <= 0)
    return;

  // Not an else-chain: one meter may feed more than one place.
  auto const &r = _vuRouting;

  for (std::size_t i = 0; i < r.channelInputs.size (); ++i)
    {
      if (number == r.channelInputs[i].left)
        routeChannelSide (i, 0, peak, rms);
      if (number == r.channelInputs[i].right)
        routeChannelSide (i, 1, peak, rms);
    }

  if (number == r.glow)
    _listener.onSubwooferVU (peak, rms);

  for (std::size_t i = 0; i < r.towers.size (); ++i)
    if (number == r.towers[i])
      _listener.onSpeakerVU (static_cast<int> (i), peak, rms);

  for (std::size_t i = 0; i < r.masterColumn.size (); ++i)
    if (number == r.masterColumn[i])
      _listener.onOutputVU (static_cast<int> (i), peak, rms);
}

void
OscMessageHandler::routeChannelSide (std::size_t channel, std::size_t side,
                                     float peak, float rms)
{
  auto &sides = _channelSides[channel];
  sides[side] = { peak, rms };

  // Peak and RMS each the louder side's, as a DJ mixer shows a stereo
  // channel -- not a downmix, which reads 3 dB low on a centred signal.
  _listener.onChannelVU (static_cast<int> (channel),
                         std::max (sides[0].peak, sides[1].peak),
                         std::max (sides[0].rms, sides[1].rms));
}

void
OscMessageHandler::handleMessage (juce::OSCMessage const &message,
                                  int clockMode)
{
  auto const address = message.getAddressPattern ().toString ();

  if (address.startsWith (_addresses.vuPrefix))
    {
      auto const number
          = address.substring (_addresses.vuPrefix.length ()).getIntValue ();

      if (message.size () < 2)
        return;

      float const peak = message[0].getFloat32 ();
      float const rms = message[1].getFloat32 ();

      routeMeter (number, peak, rms);
      return;
    }

  if (address == _addresses.energyRms)
    {
      // A message of the wrong length would leave part of the map holding
      // values from an earlier frame — energy that is no longer there.
      if (message.size () != energyGridPointCount)
        return;

      std::array<float, energyGridPointCount> values;
      for (int i = 0; i < energyGridPointCount; ++i)
        {
          if (!message[i].isFloat32 ())
            return;
          values[static_cast<size_t> (i)] = message[i].getFloat32 ();
        }

      _listener.onEnergyGrid (values.data (), energyGridPointCount);

      return;
    }

  if (address == _addresses.beatIn && message.size () >= 3)
    {
      auto const getIntArg = [] (juce::OSCArgument const &arg) -> int {
        if (arg.isInt32 ())
          return arg.getInt32 ();
        if (arg.isFloat32 ())
          return static_cast<int> (arg.getFloat32 ());
        return 0;
      };

      int const beat = getIntArg (message[0]);
      int const bar = getIntArg (message[1]);
      // A float, kept as one. Read as an int it lost its fraction -- 117.454
      // became 117 -- and a clock running a fraction of a per cent slow
      // against the beats it is sent drifts a whole beat in a few minutes.
      float const bpm = message[2].isFloat32 ()
                            ? message[2].getFloat32 ()
                            : static_cast<float> (getIntArg (message[2]));

      if (BeatTrace::device ().isEnabled ())
        BeatTrace::device ().record ("handled", beat, bar, bpm);

      _listener.onExternalBeatClock (beat, bar, bpm);

      if (clockMode != 0)
        {
          // Followed, not taken: an unsteady source must not run every clip
          // at double speed for a beat (a3-motion-ui#36).
          _engine.setTempoBPM (_externalTempo.onBeat (bpm));
          _listener.onExternalBeatSync (beat, _engine.getBeatsPerBar ());
        }
      else
        // On the internal clock: the next time it follows, it starts afresh
        // rather than measuring against a tempo from before.
        _externalTempo.reset ();

      return;
    }

  // The per-channel values coming back from A3 Core -- the answer to
  // /state/recall. Last of the four because it is the rarest: the VU meters
  // and the energy grid arrive continuously, these only when Core is asked.
  //
  // Built per channel from the address table rather than matched by prefix.
  // The addresses are configurable and {ch} may sit anywhere in them, so the
  // only honest comparison is against what the sender itself would have
  // built. Four channels times five patterns is twenty string compares, paid
  // only by messages that got past the three checks above -- which, on a
  // running rig, is almost nothing.
  if (message.size () >= 1 && message[0].isFloat32 ())
    {
      using Value = Listener::ChannelValue;

      std::array<std::pair<juce::String const *, Value>, 5> const carried{ {
          { &_addresses.channelAzimuth, Value::Azimuth },
          { &_addresses.channelElevation, Value::Elevation },
          { &_addresses.channelFilterFrequency, Value::Pot1 },
          { &_addresses.channelFilterQ, Value::Pot2 },
          { &_addresses.channelThreeD, Value::ThreeD },
      } };

      auto const value = message[0].getFloat32 ();

      for (index_t channel = 0; channel < _engine.getNumChannels (); ++channel)
        {
          auto const number = static_cast<int> (channel);

          for (auto const &[pattern, which] : carried)
            if (address == withChannelIndex (*pattern, number))
              {
                _listener.onChannelValue (number, which, value);
                return;
              }
        }

      // And the channel strip, which A3 Core relays back from REAPER: gain,
      // the three bands and volume. Walked after the five above rather than
      // merged with them because they are a different thing -- those are the
      // room, these are a mixing desk -- and because they are rarer still.
      //
      // The slot goes out as an index into _addresses.mixerChannel. The
      // listener turns it into a MixerControl through mixerControlOrder,
      // which is what that table is indexed by; making that trip here would
      // mean this file including the ui's control list to learn a name it
      // immediately hands on.
      for (index_t channel = 0; channel < _engine.getNumChannels (); ++channel)
        {
          auto const number = static_cast<int> (channel);

          for (std::size_t slot = 0; slot < _addresses.mixerChannel.size ();
               ++slot)
            if (address
                == withChannelIndex (_addresses.mixerChannel[slot], number))
              {
                _listener.onMixerChannelValue (number,
                                               static_cast<int> (slot), value);
                return;
              }
        }

      // The summing section and the shared filter. Not per channel, so no
      // withChannel and no loop over four -- and walked after the channel
      // tables because /master/volume and /channel/0/volume are one word
      // apart and the more specific has to be asked first.
      for (std::size_t slot = 0; slot < _addresses.mixerMaster.size (); ++slot)
        if (address == _addresses.mixerMaster[slot])
          {
            _listener.onMasterValue (static_cast<int> (slot), value);
            return;
          }

      for (std::size_t slot = 0; slot < _addresses.mixerFilter.size (); ++slot)
        if (address == _addresses.mixerFilter[slot])
          {
            _listener.onFilterValue (static_cast<int> (slot), value);
            return;
          }
    }
}

}
