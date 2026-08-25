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

#include "SphereProjection.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

Pos
discToDirection (Pos const &flat)
{
  auto const radius = std::hypot (flat.x (), flat.y ());

  if (radius > 1.f)
    {
      // Held at the horizon: past the rim there is nothing above it to point
      // at, and the square root below would go imaginary.
      return Pos::fromCartesian (flat.x () / radius, flat.y () / radius, 0.f);
    }

  return Pos::fromCartesian (
      flat.x (), flat.y (),
      std::sqrt (std::max (0.f, 1.f - radius * radius)));
}

Pos
directionToDisc (Pos const &direction)
{
  return Pos::fromCartesian (direction.x (), direction.y (), 0.f);
}

}
