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

#include "Glide.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
struct Vec
{
  float x, y, z;
};

Vec
vecOf (Pos p)
{
  return { p.x (), p.y (), p.z () };
}

float
lengthOf (Vec v)
{
  return std::hypot (v.x, v.y, v.z);
}

Vec
scaled (Vec v, float s)
{
  return { v.x * s, v.y * s, v.z * s };
}

Vec
sum (Vec a, Vec b)
{
  return { a.x + b.x, a.y + b.y, a.z + b.z };
}

float
dot (Vec a, Vec b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec
cross (Vec a, Vec b)
{
  return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
           a.x * b.y - a.y * b.x };
}

/** A direction at right angles to `u`, for turning half way round it. */
Vec
perpendicularTo (Vec u)
{
  auto axis = cross (u, { 0.f, 0.f, 1.f });
  if (lengthOf (axis) < 1e-3f)
    axis = cross (u, { 0.f, 1.f, 0.f });
  return scaled (axis, 1.f / lengthOf (axis));
}

/** From `a` to `b` along the sphere: the direction turns on a great circle,
 *  the distance from the listener goes straight from one to the other. */
Pos
alongTheSphere (Pos a, Pos b, float t)
{
  auto const va = vecOf (a);
  auto const vb = vecOf (b);
  auto const ra = lengthOf (va);
  auto const rb = lengthOf (vb);
  auto const straight = sum (va, scaled (sum (vb, scaled (va, -1.f)), t));
  if (ra < 1e-6f || rb < 1e-6f)
    return Pos::fromCartesian (straight.x, straight.y, straight.z);

  auto const ua = scaled (va, 1.f / ra);
  auto const ub = scaled (vb, 1.f / rb);
  auto const angle = std::acos (std::clamp (dot (ua, ub), -1.f, 1.f));
  if (angle < 1e-4f)
    return Pos::fromCartesian (straight.x, straight.y, straight.z);

  Vec u;
  if (std::sin (angle) < 1e-3f)
    {
      // Opposite points: no one great circle joins them, so take one.
      auto const turn = t * angle;
      auto const side = cross (perpendicularTo (ua), ua);
      u = sum (scaled (ua, std::cos (turn)), scaled (side, std::sin (turn)));
    }
  else
    u = scaled (sum (scaled (ua, std::sin ((1.f - t) * angle)),
                     scaled (ub, std::sin (t * angle))),
                1.f / std::sin (angle));

  auto const r = ra + (rb - ra) * t;
  return Pos::fromCartesian (u.x * r, u.y * r, u.z * r);
}
}

index_t
glideTicks (float fadeReach, index_t passTicks, index_t beatTicks)
{
  auto const half = passTicks / 2;
  auto const reach = std::clamp (fadeReach, 0.f, 1.f);
  auto const faded = static_cast<index_t> (
      std::lround (reach * static_cast<float> (half)));
  return std::max (std::min (beatTicks, half), faded);
}

Glide
startGlide (Pos from, index_t ticks)
{
  return { from, ticks, ticks };
}

Pos
glidedPosition (Glide &glide, Pos target)
{
  if (glide.ticksLeft == 0 || glide.total == 0)
    return target;

  --glide.ticksLeft;
  auto const t = 1.f
                 - static_cast<float> (glide.ticksLeft)
                       / static_cast<float> (glide.total);
  auto const eased = t * t * (3.f - 2.f * t);
  return alongTheSphere (glide.from, target, eased);
}

}
