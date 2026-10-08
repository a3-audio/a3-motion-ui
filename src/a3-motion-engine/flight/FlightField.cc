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

#include "FlightField.hh"

#include <cmath>

namespace a3
{

Vec2
clampLength (Vec2 v, float maxLength)
{
  auto const length = v.getDistanceFromOrigin ();
  if (length <= maxLength || length <= 0.f)
    return v;
  return v * (maxLength / length);
}

Vec2
gravityAt (Vec2 p, FlightBodies const &bodies, float pulse,
           FlightTuning const &tuning)
{
  auto const softeningSquared = flightSofteningSquared (tuning);
  Vec2 sum;
  for (auto i = 0; i < bodies.count && i < maxFlightBodies; ++i)
    {
      auto const &body = bodies.body[static_cast<size_t> (i)];
      auto const towards = body.at - p;
      auto const r2 = towards.getDistanceSquaredFromOrigin () + softeningSquared;
      sum += towards * (body.mass / (r2 * std::sqrt (r2)));
    }
  return clampLength (sum * (pulse * tuning.gravity), tuning.gravityMax);
}

Vec2
deadZonePush (Vec2 p, FlightBodies const &bodies, FlightTuning const &tuning)
{
  constexpr float noDirection = 1e-6f;
  Vec2 sum;
  for (auto i = 0; i < bodies.count && i < maxFlightBodies; ++i)
    {
      auto const &body = bodies.body[static_cast<size_t> (i)];
      if (body.mass >= 0.f)
        continue;
      auto const away = p - body.at;
      auto const distance = away.getDistanceFromOrigin ();
      if (distance >= tuning.deadZoneClearance || distance <= noDirection)
        continue;
      sum += away
             * (tuning.deadZoneStiffness
                * (tuning.deadZoneClearance - distance) / distance);
    }
  return sum;
}

}
