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

#include "OscTruth.hh"

namespace a3
{

/** Where this device sends and listens, out of the one truth (a3-osc.json).
 *
 *  Two destinations, and which is which is the whole point of this struct
 *  existing rather than an int being read where it is needed. `core` is
 *  A3 Core: the spatial position, and everything the mixer turns.
 *  `beatclock` is the beat-analyzer, a different process, which is told the
 *  beat and the clock mode and nothing else. Getting that backwards costs
 *  nothing visible: OSC over UDP never answers, so a message sent to the
 *  wrong one of the two is discarded in silence.
 *
 *  A listener the truth does not have is port -1 and no host: the socket
 *  does not open, and the log says so. */
struct OscEndpoints
{
  OscTruth::Endpoint core;
  OscTruth::Endpoint beatclock;

  /** Motion's own three sockets: the beat clock and Core's replies, the
   *  meters, and the IEM EnergyVisualizer. */
  int receivePort{ -1 };
  int vuPort{ -1 };
  int energyPort{ -1 };
};

/** This machine's own IPv4 addresses, as text. */
juce::StringArray ownAddresses ();

/** Core and the beat-analyzer listen on "any" in the truth, on the Core's
 *  machine. Seen from there that is this machine; seen from anywhere else
 *  (Motion on a machine of its own, 2026-10-06) it is the truth's core host.
 *  `own` says where this process runs. */
OscEndpoints oscEndpointsFrom (OscTruth const &truth,
                               juce::StringArray const &own = ownAddresses ());

}
