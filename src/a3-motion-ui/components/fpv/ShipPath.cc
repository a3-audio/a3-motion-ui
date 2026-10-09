/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include "ShipPath.hh"

#include <cmath>

namespace a3
{

ShipPathShown
shipPathShown (ShipPathFacts const &facts)
{
  if (!facts.fpv)
    return ShipPathShown::Trajectory;
  if (facts.inGame)
    return ShipPathShown::None;
  if (!facts.orbit)
    return ShipPathShown::Trajectory;
  return facts.escorting ? ShipPathShown::Escort : ShipPathShown::Orbit;
}

bool
drawsTrajectory (ShipPathShown shown)
{
  return shown == ShipPathShown::Trajectory;
}

float
lineLevelOf (ShipPathShown shown)
{
  return shown == ShipPathShown::Trajectory ? 1.f : orbitLineLevel;
}

std::vector<Vec2>
escortPathPoints (Vec2 centre, float radius, int count)
{
  if (!(radius > 0.f) || count < 3)
    return {};

  std::vector<Vec2> points;
  points.reserve (static_cast<size_t> (count) + 1);
  for (auto i = 0; i < count; ++i)
    {
      auto const a = 2.f * juce::MathConstants<float>::pi
                     * static_cast<float> (i) / static_cast<float> (count);
      auto p = centre + Vec2{ std::cos (a), std::sin (a) } * radius;
      auto const out = p.getDistanceFromOrigin ();
      if (out > 1.f)
        p = p / out;
      points.push_back (p);
    }
  points.push_back (points.front ());
  return points;
}

std::vector<Vec2>
shipPathPoints (ShipPathShown shown, std::vector<Vec2> const &guide,
                FlightBodies const &bodies, int escortId,
                FlightTuning const &tuning)
{
  if (shown == ShipPathShown::Orbit)
    return guide;
  if (shown != ShipPathShown::Escort)
    return {};
  for (auto i = 0; i < bodies.count; ++i)
    {
      auto const &body = bodies.body[static_cast<size_t> (i)];
      if (body.id == escortId)
        return escortPathPoints (body.at, escortRadius (body.mass, tuning),
                                 escortPathPointCount);
    }
  return {};
}

}
