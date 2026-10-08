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

#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-engine/flight/FlightVec.hh>

namespace a3
{

/** A ship on the floor: where it is and how it moves (floor units per beat). */
struct ShipState
{
  Vec2 p;
  Vec2 v;
};

/** What acts on a ship for one step: the point it steers for, and the pulls
 *  the world adds on top (gravity of the bodies, the push of the other ships). */
struct ShipForces
{
  OrbitPoint goal;
  Vec2 gravity;
  Vec2 separation;
};

/** One fixed step of `dt` beats: steer to the goal, add the given pulls,
 *  damp, clamp the speed to [speedMin, speedMax], keep inside the room.
 *
 *  Semi-implicit Euler (velocity first, then position with the new velocity),
 *  stable at one clock tick. Never NaN: a velocity too small to have a
 *  direction takes the goal's, or +x when the ship sits on its goal. No
 *  allocation, no lock: safe on the clock thread. */
ShipState stepShip (ShipState s, ShipForces const &f, float dt,
                    FlightTuning const &tuning);

}
