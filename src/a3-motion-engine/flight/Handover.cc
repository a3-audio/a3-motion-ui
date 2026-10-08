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

#include "Handover.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{

constexpr float shortestBlend = 1e-4f;

Pos
scaled (Pos const &p, float factor)
{
  return Pos::fromCartesian (p.x () * factor, p.y () * factor, p.z () * factor);
}

/** `p` at unit length, or invalid when it has no direction. */
Pos
unit (Pos const &p)
{
  if (!p.isValid ())
    return Pos::invalid;
  auto const length = p.distance ();
  if (length < shortestBlend)
    return Pos::invalid;
  return scaled (p, 1.f / length);
}

}

float
smoothstep (float x)
{
  auto const t = std::clamp (x, 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

Pos
handover (Pos from, Pos to, float weight)
{
  auto const start = unit (from);
  auto const end = unit (to);
  if (!start.isValid () || !end.isValid ())
    return to;

  auto const w = std::clamp (weight, 0.f, 1.f);
  auto const blend = scaled (start, 1.f - w) + scaled (end, w);
  auto const length = blend.distance ();
  if (length < shortestBlend)
    return end;
  return scaled (blend, 1.f / length);
}

}
