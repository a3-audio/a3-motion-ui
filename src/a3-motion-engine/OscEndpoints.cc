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

OscEndpoints
oscEndpointsFrom (OscTruth const &truth)
{
  auto const nowhere = OscTruth::Endpoint{};

  OscEndpoints endpoints;
  endpoints.core = truth.endpoint ("core", "osc").value_or (nowhere);
  endpoints.beatclock
      = truth.endpoint ("beat-analyzer", "clock").value_or (nowhere);
  endpoints.receivePort = truth.port ("motion", "osc");
  endpoints.vuPort = truth.port ("motion", "vu");
  endpoints.energyPort = truth.port ("motion", "energy");
  return endpoints;
}

}
