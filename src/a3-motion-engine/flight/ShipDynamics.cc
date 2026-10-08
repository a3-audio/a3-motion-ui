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

#include "ShipDynamics.hh"

#include <a3-motion-engine/flight/FlightField.hh>

namespace a3
{

namespace
{

/** Below this a vector has no usable direction. */
constexpr float noDirection = 1e-6f;

/** PD steering towards the goal and its velocity, capped at steerMax so a
 *  heavy group close by can out-pull it for a moment. */
Vec2
steerTowards (ShipState const &s, OrbitPoint const &goal,
              FlightTuning const &tuning)
{
  auto const towards = (goal.at - s.p) * tuning.steerStiffness
                       + (goal.velocity - s.v) * tuning.steerDamping;
  return clampLength (towards, tuning.steerMax);
}

/** The soft wall: nothing inside rimSoft, a spring back towards the centre
 *  beyond it. */
Vec2
rimPush (Vec2 p, FlightTuning const &tuning)
{
  auto const radius = p.getDistanceFromOrigin ();
  if (radius <= tuning.rimSoft)
    return {};
  return p * (-tuning.rimStiffness * (radius - tuning.rimSoft) / radius);
}

/** `v` with its length held to [speedMin, speedMax]. A velocity without a
 *  direction takes `fallback`'s, and +x when that has none either. */
Vec2
limitSpeed (Vec2 v, Vec2 fallback, FlightTuning const &tuning)
{
  auto const speed = v.getDistanceFromOrigin ();
  if (speed > tuning.speedMax)
    return v * (tuning.speedMax / speed);
  if (speed >= tuning.speedMin)
    return v;
  if (speed > noDirection)
    return v * (tuning.speedMin / speed);

  auto const fallbackLength = fallback.getDistanceFromOrigin ();
  if (fallbackLength > noDirection)
    return fallback * (tuning.speedMin / fallbackLength);
  return { tuning.speedMin, 0.f };
}

/** The hard wall at radius 1: back onto the rim, the outward part of the
 *  velocity taken away, the rest sliding along the rim. */
ShipState
keepInsideTheRoom (ShipState s, FlightTuning const &tuning)
{
  auto const radius = s.p.getDistanceFromOrigin ();
  if (radius <= 1.f)
    return s;

  auto const outward = s.p / radius;
  auto const outwardSpeed = s.v.getDotProduct (outward);
  if (outwardSpeed > 0.f)
    s.v -= outward * outwardSpeed;
  Vec2 const alongTheRim{ -outward.y, outward.x };
  return { outward, limitSpeed (s.v, alongTheRim, tuning) };
}

}

ShipState
stepShip (ShipState s, ShipForces const &f, float dt,
          FlightTuning const &tuning)
{
  auto const acceleration = steerTowards (s, f.goal, tuning) + f.gravity
                            + f.separation - s.v * tuning.damping
                            + rimPush (s.p, tuning);
  s.v = limitSpeed (s.v + acceleration * dt, f.goal.at - s.p, tuning);
  s.p += s.v * dt;
  return keepInsideTheRoom (s, tuning);
}

}
