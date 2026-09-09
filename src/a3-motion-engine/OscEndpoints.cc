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
loadOscEndpoints (juce::var const &config)
{
  OscEndpoints endpoints;

  auto const block = config["oscSender"];
  if (!block.isObject ())
    return endpoints;

  auto const host = block["host"].toString ();
  if (host.isNotEmpty ())
    endpoints.host = host;

  if (block.hasProperty ("port"))
    endpoints.corePort = static_cast<int> (block["port"]);

  // The clock's own port is optional, and its absence means the file predates
  // the split rather than that the clock goes nowhere.
  endpoints.beatclockPort = block.hasProperty ("beatclockPort")
                                ? static_cast<int> (block["beatclockPort"])
                                : endpoints.corePort;

  return endpoints;
}

}
