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

#include <array>

namespace a3
{

OscMessageHandler::OscMessageHandler (MotionEngine &engine, Listener &listener)
    : _engine (engine), _listener (listener)
{
}

void
OscMessageHandler::setAddresses (OscAddresses const &addresses)
{
  _addresses = addresses;
}

void
OscMessageHandler::handleMessage (juce::OSCMessage const &message,
                                  int clockMode)
{
  auto const address = message.getAddressPattern ().toString ();

  if (address.startsWith (_addresses.vuPrefix))
    {
      auto const channelStr = address.substring (_addresses.vuPrefix.length ());
      auto const channel = channelStr.getIntValue ();

      if (message.size () < 2)
        return;

      float const peak = message[0].getFloat32 ();
      float const rms = message[1].getFloat32 ();

      if (channel >= 0 && channel <= 3)
        _listener.onChannelVU (channel, peak, rms);
      else if (channel == 4)
        _listener.onSubwooferVU (peak, rms);
      else if (channel >= 5 && channel <= 8)
        _listener.onSpeakerVU (channel - 5, peak, rms);

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
      int const bpm = getIntArg (message[2]);

      _listener.onExternalBeatClock (beat, bar, static_cast<float> (bpm));

      if (clockMode != 0)
        {
          _engine.setTempoBPM (static_cast<float> (bpm));
          _listener.onExternalBeatSync (beat, _engine.getBeatsPerBar ());
        }

      return;
    }

  // The position, coming back from A3 Core -- the answer to /state/recall.
  // Last of the four because it is the rarest: the VU meters and the energy
  // grid arrive continuously, a position only when Core is asked.
  //
  // Built per channel from the address table rather than matched by prefix.
  // The addresses are configurable and {ch} may sit anywhere in them, so the
  // only honest comparison is against what the sender itself would have
  // built. Four channels times two patterns is eight string compares, paid
  // only by messages that got past the three checks above.
  if (message.size () >= 1 && message[0].isFloat32 ())
    {
      auto const value = message[0].getFloat32 ();

      for (index_t channel = 0; channel < _engine.getNumChannels (); ++channel)
        {
          auto const number = static_cast<int> (channel);

          if (address == withChannel (_addresses.channelAzimuth, number))
            {
              _listener.onChannelAzimuth (number, value);
              return;
            }

          if (address == withChannel (_addresses.channelElevation, number))
            {
              _listener.onChannelElevation (number, value);
              return;
            }
        }
    }
}

}
