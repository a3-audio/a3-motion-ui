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

#include "SpatBackendIEM.hh"

#include <JuceHeader.h>

namespace a3
{

SpatBackendIEM::SpatBackendIEM (juce::String address, int basePort)
    : _address (address), _basePort (basePort)
{
  _sender.connect (address, basePort);
}

void
SpatBackendIEM::sendChannelPosition (index_t channel, Pos const &pos)
{
  juce::OSCBundle bundle;

  auto message = juce::OSCMessage ("/StereoEncoder/azimuth", pos.azimuth ());
  bundle.addElement ({ message });

  message = juce::OSCMessage ("/StereoEncoder/elevation", pos.elevation ());
  bundle.addElement ({ message });

  jassert (channel <= std::numeric_limits<int>::max ());
  _sender.sendToIPAddress (_address, _basePort + static_cast<int> (channel),
                           bundle);
}

void
SpatBackendIEM::sendChannelWidth (index_t channel, float width)
{
  throw std::runtime_error ("SpatBackendIEM::sendChannelWidth: implement me!");
}

}
