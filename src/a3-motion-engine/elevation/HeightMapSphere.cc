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

#include "HeightMapSphere.hh"

#include <cmath>

#include <a3-motion-engine/util/Geometry.hh>

namespace a3
{

float
HeightMapSphere::computeHeight (Pos const &pos) const
{
  auto zSquared = 1.f - pos.x () * pos.x () - pos.y () * pos.y ();

  if (zSquared > 0.f)
    return std::sqrt (zSquared);

  return 0.f;
}

Pos
HeightMapSphere::mapTo3D (Pos const &pos2D) const
{
  auto const x = pos2D.x ();
  auto const y = pos2D.y ();
  auto const r = std::sqrt (x * x + y * y);

  // Clamp to unit disc
  if (r < 1e-6f)
    return Pos::fromCartesian (0.f, 0.f, 1.f); // north pole

  auto const cov = _coverage.load (std::memory_order_relaxed);

  // θ_max = coverage × π
  // coverage 0.5 → π/2 (hemisphere, equator at rim)
  // coverage 1.0 → π   (full sphere, south pole at rim)
  // coverage 0.33 → ~60° (top third)
  auto const thetaMax = cov * pi<float> ();

  // Map radial distance to colatitude.
  // r ∈ [0,1] → θ ∈ [0, θ_max]  (within the sphere disc)
  // r > 1.0 → θ extends past θ_max, wrapping the ball onto the back
  // of the sphere (underside).  Capped at π (south pole).
  auto const theta = std::min (r * thetaMax, pi<float> ());

  // Azimuth angle preserved from 2D position
  auto const phi = std::atan2 (y, x);

  // Spherical to Cartesian (unit sphere)
  auto const sinTheta = std::sin (theta);
  auto const cosTheta = std::cos (theta);
  auto const newX = sinTheta * std::cos (phi);
  auto const newY = sinTheta * std::sin (phi);
  auto const newZ = cosTheta;

  return Pos::fromCartesian (newX, newY, newZ);
}

void
HeightMapSphere::setCoverage (float coverage)
{
  _coverage.store (std::clamp (coverage, 0.05f, 1.0f),
                   std::memory_order_relaxed);
}

float
HeightMapSphere::getCoverage () const
{
  return _coverage.load (std::memory_order_relaxed);
}

}
