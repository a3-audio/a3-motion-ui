/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class Pattern;

/** How far the figure's plane is leant in the room (2026-09-27), each
 *  bipolar with zero upright and a quarter turn at either end. Applied to a
 *  direction on the sphere, after the height map: tilt about the left-right
 *  axis, positive taking the front down; roll about the front-back axis,
 *  positive taking the left down. x is front, y left, z up. */
struct SpaceTurn
{
  float tilt = 0.f;
  float roll = 0.f;
};

/** A direction on the sphere, leant: tilt first, then roll. */
Pos turnedInSpace (Pos const &direction, SpaceTurn turn);

/** And back -- where a finger lands in the leant plane, as the figure has it
 *  before it is leant. */
Pos unturnedInSpace (Pos const &direction, SpaceTurn turn);

/** What a clip leans by right now: its tilt and roll, each swept out and back
 *  by its own sweep. The one call the engine and every drawing make. */
SpaceTurn spaceTurnOf (Pattern const &pattern);

}
