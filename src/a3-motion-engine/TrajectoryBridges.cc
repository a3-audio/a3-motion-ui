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

#include "TrajectoryBridges.hh"

#include <a3-motion-engine/TrajectoryShape.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
float
distanceBetween (Pos const &a, Pos const &b)
{
  return std::sqrt (std::pow (b.x () - a.x (), 2.f)
                    + std::pow (b.y () - a.y (), 2.f)
                    + std::pow (b.z () - a.z (), 2.f));
}

/** Normalised coordinates run -1 to 1, so the widest gap there can be is 2. */
constexpr float sphereDiameter = 2.f;
}

bool
BridgePlan::bridged (index_t tick) const
{
  return via (tick).has_value ();
}

std::optional<index_t>
BridgePlan::via (index_t tick) const
{
  auto const found
      = std::find_if (bridges.begin (), bridges.end (),
                      [tick] (Bridge const &b) { return b.fromTick == tick; });

  if (found == bridges.end ())
    return std::nullopt;

  return found->viaTick;
}

BridgePlan
planBridges (std::vector<Pos> const &ticks, float fadeReach, int bridgeBias,
             juce::int64 seed)
{
  BridgePlan plan;
  if (ticks.size () < 2)
    return plan;

  auto const reach = juce::jlimit (0.f, 1.f, fadeReach) * sphereDiameter;
  if (reach <= 0.f)
    return plan;

  for (auto const at : trajectoryJumps (ticks))
    {
      auto const next = (at + 1) % ticks.size ();
      if (distanceBetween (ticks[at], ticks[next]) > reach)
        continue;

      plan.bridges.push_back (
          { static_cast<index_t> (at), static_cast<index_t> (next) });
    }

  return plan;
}

}
