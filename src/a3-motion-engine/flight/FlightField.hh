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

#pragma once

#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-engine/flight/FlightVec.hh>

#include <array>

namespace a3
{

constexpr int maxFlightBodies = 8;

/** A group of guests on the floor, acting as a planet: a gravity well at `at`.
 *  A negative mass is a dead zone, which pushes instead of pulls. */
struct FlightBody
{
  Vec2 at;
  float mass = 1.f;
};

/** Fixed size, so the clock thread can hold a copy without allocating. Only
 *  the first `count` entries are bodies. */
struct FlightBodies
{
  std::array<FlightBody, maxFlightBodies> body{};
  int count = 0;
};

/** `v` shortened to `maxLength` if it is longer; unchanged otherwise, and
 *  unchanged when it is zero. */
Vec2 clampLength (Vec2 v, float maxLength);

/** The pull of every body at `p`, scaled by `pulse`, capped at gravityMax.
 *
 *  Plummer-softened: the softening length keeps the pull finite on top of a
 *  body (it is zero there) and its peak bounded just beside it, so a ship
 *  flying straight over a group is bent, not flung to infinity. */
Vec2 gravityAt (Vec2 p, FlightBodies const &bodies, float pulse,
                FlightTuning const &tuning);

/** The walls round the dead zones at `p`: nothing beyond deadZoneClearance
 *  of one, a spring straight away from it inside. Not capped by gravityMax
 *  and not breathing with the beat, as the room's rim is not: "not here"
 *  holds whatever pulls the other way. Zero on the very centre of a zone
 *  (no direction to push in); the zone's own repulsion moves a ship off it. */
Vec2 deadZonePush (Vec2 p, FlightBodies const &bodies,
                   FlightTuning const &tuning);

}
