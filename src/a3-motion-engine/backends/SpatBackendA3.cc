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

#include "SpatBackendA3.hh"

#include <JuceHeader.h>

namespace a3
{

SpatBackendA3::SpatBackendA3 (juce::String address, int port)
    : _address (address), _port (port)
{
  _sender.connect (address, port);
}

void
SpatBackendA3::sendChannelPosition (int index, Pos const &pos)
{
  juce::OSCBundle bundle;

  auto const azimuthPattern = juce::String("/channel/") + juce::String(index) + "/azimuth";
  auto message = juce::OSCMessage (azimuthPattern, pos.azimuth ());
  bundle.addElement ({ message });

  auto const elevationPattern = juce::String("/channel/") + juce::String(index) + "/elevation";
  message = juce::OSCMessage (elevationPattern, pos.elevation ());
  bundle.addElement ({ message });

  juce::Logger::writeToLog(azimuthPattern);
  juce::Logger::writeToLog(elevationPattern);

  _sender.sendToIPAddress (_address, _port, bundle);
}

}
