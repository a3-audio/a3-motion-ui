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

namespace a3
{

OscMessageHandler::OscMessageHandler (MotionEngine &engine, Listener &listener)
    : _engine (engine), _listener (listener)
{
}

void
OscMessageHandler::handleMessage (juce::OSCMessage const &message,
                                  int clockMode)
{
  auto const address = message.getAddressPattern ().toString ();

  if (address.startsWith ("/vu/"))
    {
      auto const channelStr = address.substring (4);
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

  if (address == "/beat" && message.size () >= 3)
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
}

}
