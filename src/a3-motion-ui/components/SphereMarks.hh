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

namespace a3
{

/** How wide a shape's dot is drawn on the sphere, in the sphere's own units
 *  (its radius is 1). Three times the line, as it always was -- but never
 *  less than a size of its own: the line went thin when it moved to the GPU
 *  (0.0018 by default), and three of it was a dot of one pixel. */
constexpr float minimumJumpDotDiameter = 0.035f;

constexpr float
jumpDotDiameter (float lineThickness)
{
  return lineThickness * 3.f > minimumJumpDotDiameter ? lineThickness * 3.f
                                                     : minimumJumpDotDiameter;
}

}
