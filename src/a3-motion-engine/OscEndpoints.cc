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


#include "OscEndpoints.hh"

namespace a3
{

juce::StringArray
ownAddresses ()
{
  juce::StringArray addresses;
  for (auto const &address : juce::IPAddress::getAllAddresses (false))
    addresses.add (address.toString ());
  return addresses;
}

namespace
{
/** An endpoint on the Core's machine, reached from wherever this runs: "any"
 *  read as local is right only there; elsewhere it is the core host. */
OscTruth::Endpoint
onTheCoresMachine (OscTruth const &truth, OscTruth::Endpoint endpoint,
                   juce::StringArray const &own)
{
  auto const coreHost = truth.host ("core");
  auto const awayFromTheCore
      = coreHost.isNotEmpty () && !own.contains (coreHost);
  if (awayFromTheCore && endpoint.host == truth.host ("local"))
    endpoint.host = coreHost;
  return endpoint;
}
}

OscEndpoints
oscEndpointsFrom (OscTruth const &truth, juce::StringArray const &own)
{
  OscEndpoints endpoints;
  if (auto const core = truth.endpoint ("core", "osc"))
    endpoints.core = onTheCoresMachine (truth, *core, own);
  if (auto const clock = truth.endpoint ("beat-analyzer", "clock"))
    endpoints.beatclock = onTheCoresMachine (truth, *clock, own);
  endpoints.receivePort = truth.port ("motion", "osc");
  endpoints.vuPort = truth.port ("motion", "vu");
  endpoints.energyPort = truth.port ("motion", "energy");
  return endpoints;
}

}
