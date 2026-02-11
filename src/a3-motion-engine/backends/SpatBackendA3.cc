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
#include <cmath>

namespace a3
{

SpatBackendA3::SpatBackendA3 (juce::String address, int port)
    : _address (address), _port (port)
{
  if (_sender.connect (address, port))
    std::cout << "OSC Sender connected to " << address << ":" << port << std::endl;
  else
    std::cerr << "ERROR: OSC Sender failed to connect to " << address << ":" << port << std::endl;

  // Pre-cache OSC address patterns to avoid heap allocation per send
  for (int ch = 0; ch < kMaxChannels; ++ch)
    {
      auto prefix = juce::String ("/channel/") + juce::String (ch);
      _azimuthPatterns[ch]   = prefix + "/azimuth";
      _elevationPatterns[ch] = prefix + "/elevation";
      _pot1Patterns[ch]      = prefix + "/pot_1";
      _pot2Patterns[ch]      = prefix + "/pot_2";
    }
}

void
SpatBackendA3::sendPosition (index_t channel, Pos const &pos)
{
  jassert (channel < kMaxChannels);
  
  float az = pos.azimuth();
  float el = pos.elevation();
  
  // Deduplicate: skip if values haven't changed significantly
  float dAz = std::abs(az - _lastAzimuth[channel]);
  float dEl = std::abs(el - _lastElevation[channel]);
  
  if (dAz < kAngleTolerance && dEl < kAngleTolerance)
    return;  // Skip duplicate send
  
  _lastAzimuth[channel] = az;
  _lastElevation[channel] = el;
  
  juce::OSCBundle bundle;

  auto message = juce::OSCMessage (_azimuthPatterns[channel], az);
  bundle.addElement ({ message });

  message = juce::OSCMessage (_elevationPatterns[channel], el);
  bundle.addElement ({ message });

  if (!_sender.sendToIPAddress (_address, _port, bundle))
    std::cerr << "OSC send failed for channel " << channel << std::endl;
}

void
SpatBackendA3::sendPot1 (index_t channel, float pot1)
{
  jassert (channel < kMaxChannels);
  auto message = juce::OSCMessage (_pot1Patterns[channel], pot1);
  _sender.sendToIPAddress (_address, _port, message);
}

void
SpatBackendA3::sendPot2 (index_t channel, float pot2)
{
  jassert (channel < kMaxChannels);
  auto message = juce::OSCMessage (_pot2Patterns[channel], pot2);
  _sender.sendToIPAddress (_address, _port, message);
}
}
