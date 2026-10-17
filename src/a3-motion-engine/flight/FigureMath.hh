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

#include <a3-motion-engine/flight/FlightVec.hh>
#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/Types.hh>

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace a3
{

/** A ship number as an index into the per-ship arrays. */
inline size_t
shipIndex (int ship)
{
  return static_cast<size_t> (ship);
}

/** An axis number as an index into the per-axis arrays. */
inline size_t
axisIndex (int axis)
{
  return static_cast<size_t> (axis);
}

/** Degrees as radians. */
inline float
radians (float degrees)
{
  return degrees * pi<float> () / 180.f;
}

/** The point at `radius` from the middle, `angle` radians round from the
 *  floor's x axis. */
inline Vec2
polar (float angle, float radius)
{
  return { radius * std::cos (angle), radius * std::sin (angle) };
}

/** The angle of `p` round the middle, radians from the floor's x axis. */
inline float
angleOf (Vec2 p)
{
  return std::atan2 (p.y, p.x);
}

/** +1 when a ship at `p` flying with `v` circles the middle counter-clockwise,
 *  -1 clockwise: the way a figure turns it without reversing it. */
inline float
flightSense (Vec2 p, Vec2 v)
{
  return p.x * v.y - p.y * v.x < 0.f ? -1.f : 1.f;
}

/** The angle between two directions, in degrees (distances do not count). */
inline double
degreesBetween (Pos const &a, Pos const &b)
{
  auto const dot = a.x () * b.x () + a.y () * b.y () + a.z () * b.z ();
  auto const cosine = std::clamp (static_cast<double> (dot / (a.distance () * b.distance ())),
                                  -1., 1.);
  return std::acos (cosine) * 180. / juce::MathConstants<double>::pi;
}

}
