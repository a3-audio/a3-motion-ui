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

#include "ElevationSideView.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

ElevationSidePoint
elevationSideView (Pos const &direction)
{
  auto const x = direction.x ();
  auto const y = direction.y ();
  auto const z = direction.z ();

  auto const rXY = std::sqrt (x * x + y * y);

  ElevationSidePoint point;
  point.frac = std::atan2 (rXY, z) / pi<float> ();

  // Straight up or straight down: no bearing to speak of, so the middle. The
  // alternative is whatever atan2 makes of two zeroes, which is a point that
  // flicks to an edge as a figure passes the pole.
  point.across = rXY < 1e-6f ? 0.f : -y / rXY;
  point.behind = x > 0.f;

  return point;
}

std::vector<ElevationSidePoint>
elevationSideView (std::vector<Pos> const &directions, std::size_t maxPoints)
{
  std::vector<ElevationSidePoint> drawn;

  if (directions.empty () || maxPoints == 0)
    return drawn;

  // Every nth, chosen so the last tick is still reached: taking the first
  // maxPoints of a long take would draw its first twentieth and call it the
  // figure.
  auto const stride
      = std::max<std::size_t> (1, (directions.size () + maxPoints - 1)
                                      / maxPoints);

  drawn.reserve (directions.size () / stride + 1);

  for (std::size_t i = 0; i < directions.size (); i += stride)
    if (directions[i].isValid ())
      drawn.push_back (elevationSideView (directions[i]));

  return drawn;
}

}
