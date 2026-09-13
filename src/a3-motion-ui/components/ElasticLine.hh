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

#pragma once

#include <cstddef>

namespace a3
{

/** The trajectory as an elastic band with a weight on it.
 *
 *  The blob has mass (BlobInertia.hh), so it hangs behind the point on the
 *  figure it belongs to. The line is not rigid: near the blob it is pulled out
 *  of shape towards it and comes back to the figure further along.
 *
 *  That is one vector and a bump, not a second simulation. The deflection is
 *  what the engine already hands out -- where the blob is, less where the
 *  figure says it should be -- and everything here decides is *how much of the
 *  line comes along*.
 *
 *  Drawing the deflected line rather than the plain one is not the picture
 *  lying about the take. The line has always shown what is *played*, not what
 *  was recorded: the spin turns it, the squeezes press it flat, the sweeps
 *  move it. The inertia is the sixth of that family. (The first version of
 *  this refused to bend the line on the grounds that it was "the score" --
 *  a rule that does not exist, and the maintainer had to say so twice.)
 */

/** How far apart two points of a drawn figure are, counted in points.
 *
 *  A closed figure wraps: the last point and the first are neighbours, so the
 *  bulge has to reach across the seam rather than stopping dead at it. An open
 *  one does not, or a pull at the start would raise a second bulge at the far
 *  end, where there is nothing pulling. */
inline std::size_t
arcDistance (std::size_t from, std::size_t to, std::size_t count, bool closed)
{
  auto const straight = from > to ? from - to : to - from;
  if (!closed || count == 0)
    return straight;

  auto const around = count - straight;
  return straight < around ? straight : around;
}

/** How much of the deflection a point this far from the blob carries, 0..1.
 *
 *  One at the blob -- so the line passes exactly through it and the two read
 *  as attached, which is the whole point -- and nothing beyond `reach`, with
 *  no corner where it lands. A smooth bump rather than a cone: a cone leaves
 *  a visible kink at the blob, which reads as a fold in the line instead of a
 *  band under tension.
 *
 *  `reach` is in points, and zero means a rigid line: nothing moves at all,
 *  not even the point under the blob, because a deflection carried by one
 *  point alone is a spike. */
inline float
bulgeWeight (std::size_t distance, float reach)
{
  if (!(reach > 0.f))
    return 0.f;

  auto const t = static_cast<float> (distance) / reach;
  if (t >= 1.f)
    return 0.f;

  auto const falloff = 1.f - t * t;
  return falloff * falloff;
}

}
