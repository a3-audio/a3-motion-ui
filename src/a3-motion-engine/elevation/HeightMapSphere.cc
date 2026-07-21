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
  return mapTo3D (pos2D, _coverage.load (std::memory_order_relaxed));
}

Pos
HeightMapSphere::mapTo3D (Pos const &pos2D, float coverage) const
{
  auto const x = pos2D.x ();
  auto const y = pos2D.y ();
  auto const r = std::sqrt (x * x + y * y);

  // Clamp to unit disc
  if (r < 1e-6f)
    return Pos::fromCartesian (0.f, 0.f, 1.f); // north pole

  auto const cov = std::clamp (coverage, 0.05f, 1.0f);

  // Azimuth angle preserved from 2D position
  auto const phi = std::atan2 (y, x);

  // coverage 0.5 → π/2 (hemisphere), 1.0 → π (full sphere)
  auto const thetaMax = cov * pi<float> ();
  auto const halfPi = pi<float> () / 2.f;

  // The on-screen position of a sphere point is its orthographic
  // projection, i.e. sin(theta) — not r itself. A plain linear r → theta
  // mapping therefore makes the visible dot drift away from the touch
  // point (sin is concave), most noticeably in the middle of a drag.
  //
  // Instead we solve for theta so that sin(theta) tracks r as closely as
  // the geometry allows within the visible sphere (r <= 1, i.e. before the
  // touch leaves the drawn circle), exactly when coverage == 0.5 and
  // proportionally otherwise, while still landing on exactly thetaMax at
  // r=1 (so "coverage" keeps its documented meaning) and — for
  // coverage > 0.5 — still crossing the equator at exactly the same r as
  // the old linear mapping did. Beyond that point (including r > 1, i.e.
  // dragging past the drawn circle into the surrounding touch area) theta
  // keeps growing exactly like the original unbounded r * thetaMax mapping
  // did, so the "wraps under the sphere past the edge" behaviour — which
  // is reachable at any coverage by dragging far enough out — is
  // unchanged.
  float theta;
  if (thetaMax <= halfPi)
    {
      theta = (r <= 1.f)
                  ? std::asin (std::min (r * std::sin (thetaMax), 1.f))
                  : r * thetaMax;
    }
  else
    {
      auto const rEquator = halfPi / thetaMax;
      theta = (r <= rEquator) ? std::asin (std::min (r / rEquator, 1.f))
                              : r * thetaMax;
    }

  theta = (_edgeMode.load (std::memory_order_relaxed) == EdgeMode::Clamp)
              ? std::min (theta, thetaMax)
              : std::min (theta, pi<float> ());

  // Spherical to Cartesian (unit sphere)
  auto const sinTheta = std::sin (theta);
  auto const cosTheta = std::cos (theta);
  auto const newX = sinTheta * std::cos (phi);
  auto const newY = sinTheta * std::sin (phi);
  auto const newZ = cosTheta;

  return Pos::fromCartesian (newX, newY, newZ);
}

Pos
HeightMapSphere::mapTo2D (Pos const &pos3D) const
{
  return mapTo2D (pos3D, _coverage.load (std::memory_order_relaxed));
}

Pos
HeightMapSphere::mapTo2D (Pos const &pos3D, float coverage) const
{
  auto const x = pos3D.x ();
  auto const y = pos3D.y ();
  auto const z = pos3D.z ();

  auto const rXY = std::sqrt (x * x + y * y);
  auto const cov = std::clamp (coverage, 0.05f, 1.0f);
  auto const thetaMax = cov * pi<float> ();
  auto const halfPi = pi<float> () / 2.f;

  if (rXY < 1e-6f)
    {
      if (z >= 0.f)
        return Pos::fromCartesian (0.f, 0.f, 0.f); // north pole

      // Exact south pole: mapTo3D() clamps theta at pi, so every r beyond
      // that point collapses onto this same 3D point — phi and the exact
      // original r are unrecoverable. Fall back to the smallest r that
      // reaches it, so re-grabbing resumes on the back of the sphere
      // instead of snapping to the front.
      auto const rAtPole = pi<float> () / thetaMax;
      return Pos::fromCartesian (rAtPole, 0.f, 0.f);
    }

  // Colatitude recovered from the full 3D point — unlike sin(theta) (the
  // on-screen radius), atan2(rXY, z) is unambiguous between the front
  // (z >= 0) and back (z < 0) hemisphere.
  auto const theta = std::atan2 (rXY, z);
  auto const phi = std::atan2 (y, x);

  // Exact inverse of the piecewise theta(r) mapping in mapTo3D().
  float r;
  if (thetaMax <= halfPi)
    {
      r = (theta <= thetaMax) ? std::sin (theta) / std::sin (thetaMax)
                              : theta / thetaMax;
    }
  else
    {
      auto const rEquator = halfPi / thetaMax;
      r = (theta <= halfPi) ? std::sin (theta) * rEquator
                            : theta / thetaMax;
    }

  return Pos::fromCartesian (r * std::cos (phi), r * std::sin (phi), 0.f);
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

void
HeightMapSphere::setEdgeMode (EdgeMode mode)
{
  _edgeMode.store (mode, std::memory_order_relaxed);
}

HeightMapSphere::EdgeMode
HeightMapSphere::getEdgeMode () const
{
  return _edgeMode.load (std::memory_order_relaxed);
}

}
