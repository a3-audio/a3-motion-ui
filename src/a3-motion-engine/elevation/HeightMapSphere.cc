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

namespace
{

// All pattern SVGs (pattern/system/*.svg, pattern/user/*.svg) are authored
// in a -1..1 viewBox, so a recorded pattern's own 2D coordinates can reach
// up to this box's corner distance (e.g. Square's corners, Triangle's
// points) — well past r=1, the touchpad's *visible circle* radius. The
// coverage-based mapTo3D()/mapTo2D() overloads below are calibrated to
// that visible-circle r=1 (their whole purpose is the interactive
// touch-drag/EdgeMode mechanic, where r=1 is exactly where a finger leaves
// the drawn circle) and stay that way. The per-clip ElevationParams
// mapping further down instead rescales r by this constant before shaping
// it, so a clip's `reach` parameter is calibrated to the pattern
// coordinate system's own extent — otherwise any point beyond r=1 (i.e.
// most non-circular patterns) falls into the shape formula's uncorrected
// linear fallback branch and overshoots thetaMax, e.g. a Square's corners
// (r=sqrt(2)) landing well past the equator even though its edges
// correctly stop exactly at thetaMax.
constexpr float kPatternCoordinateMaxRadius = 1.41421356f; // sqrt(2)

// Shared piecewise theta(r) shape used by both the plain coverage mapping
// and the per-clip reach mapping below — see the coverage overload's
// comment (further down) for why it's piecewise rather than a plain
// r * thetaMax.
float
thetaShapeFromR (float r, float reach)
{
  auto const cov = std::clamp (reach, 0.05f, 1.0f);
  auto const thetaMax = cov * pi<float> ();
  auto const halfPi = pi<float> () / 2.f;

  if (thetaMax <= halfPi)
    return (r <= 1.f) ? std::asin (std::min (r * std::sin (thetaMax), 1.f))
                      : r * thetaMax;

  auto const rEquator = halfPi / thetaMax;
  return (r <= rEquator) ? std::asin (std::min (r / rEquator, 1.f))
                        : r * thetaMax;
}

// Exact inverse of thetaShapeFromR().
float
rFromThetaShape (float theta, float reach)
{
  auto const cov = std::clamp (reach, 0.05f, 1.0f);
  auto const thetaMax = cov * pi<float> ();
  auto const halfPi = pi<float> () / 2.f;

  if (thetaMax <= halfPi)
    return (theta <= thetaMax) ? std::sin (theta) / std::sin (thetaMax)
                               : theta / thetaMax;

  auto const rEquator = halfPi / thetaMax;
  return (theta <= halfPi) ? std::sin (theta) * rEquator : theta / thetaMax;
}

}

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
  return mapTo3D (pos2D, coverage,
                  _edgeMode.load (std::memory_order_relaxed));
}

Pos
HeightMapSphere::mapTo3D (Pos const &pos2D, float coverage,
                          EdgeMode edgeMode) const
{
  auto const x = pos2D.x ();
  auto const y = pos2D.y ();
  auto const r = std::sqrt (x * x + y * y);

  // Clamp to unit disc
  if (r < 1e-6f)
    return Pos::fromCartesian (0.f, 0.f, 1.f); // north pole

  // Azimuth angle preserved from 2D position
  auto const phi = std::atan2 (y, x);

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
  auto const thetaMax = std::clamp (coverage, 0.05f, 1.0f) * pi<float> ();
  auto theta = thetaShapeFromR (r, coverage);

  theta = (edgeMode == EdgeMode::Clamp) ? std::min (theta, thetaMax)
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
HeightMapSphere::mapTo3D (Pos const &pos2D, ElevationParams const &params) const
{
  auto const x = pos2D.x ();
  auto const y = pos2D.y ();
  auto const phi = std::atan2 (y, x);

  // frac: 0 = north pole, 1 = south pole. Strictly monotonic in r (unlike
  // the old wrap/elevation/base-point scheme this replaced) — r=0 is
  // always the pole, r=1 is always reach's point, nothing folds back.
  float frac;
  if (params.flat)
    {
      frac = std::clamp (params.flatElevation, 0.f, 1.f);
    }
  else
    {
      auto const r = std::sqrt (x * x + y * y);
      auto const rNorm = r / kPatternCoordinateMaxRadius;
      auto theta
          = std::min (thetaShapeFromR (rNorm, params.reach), pi<float> ());
      if (params.mirrorSouth)
        theta = pi<float> () - theta;
      frac = theta / pi<float> ();
    }

  // clipTop/clipBottom are a plain, final, absolute clamp — [bandLow,
  // bandHigh] as a fraction of the full north(0)-to-south(1) range — with
  // no interaction with reach/mirrorSouth's shape. If the two cross (both
  // pushed past each other) the reachable range collapses to their
  // midpoint, pinning the pattern to a single elevation line.
  auto const rangeLow = std::clamp (params.clipTop, 0.f, 1.f);
  auto const rangeHigh = 1.f - std::clamp (params.clipBottom, 0.f, 1.f);
  bool const collapsed = rangeLow >= rangeHigh;
  auto const bandLow = collapsed ? (rangeLow + rangeHigh) * 0.5f
                                 : std::min (rangeLow, rangeHigh);
  auto const bandHigh = collapsed ? bandLow : std::max (rangeLow, rangeHigh);
  frac = std::clamp (frac, bandLow, bandHigh);

  auto const thetaFinal = frac * pi<float> ();
  auto const sinTheta = std::sin (thetaFinal);
  auto const cosTheta = std::cos (thetaFinal);
  return Pos::fromCartesian (sinTheta * std::cos (phi),
                             sinTheta * std::sin (phi), cosTheta);
}

Pos
HeightMapSphere::mapTo2D (Pos const &pos3D) const
{
  return mapTo2D (pos3D, _coverage.load (std::memory_order_relaxed));
}

Pos
HeightMapSphere::mapTo2D (Pos const &pos3D, float coverage,
                          EdgeMode /*edgeMode*/) const
{
  // The inverse mapping recovers colatitude directly from the 3D point and
  // never needs to know how theta was clamped on the way in — edge mode
  // only affects the forward mapTo3D() direction. Kept as a parameter
  // purely so callers can use the same explicit (coverage, edgeMode) pair
  // for both directions.
  return mapTo2D (pos3D, coverage);
}

Pos
HeightMapSphere::mapTo2D (Pos const &pos3D, float coverage) const
{
  auto const x = pos3D.x ();
  auto const y = pos3D.y ();
  auto const z = pos3D.z ();

  auto const rXY = std::sqrt (x * x + y * y);

  if (rXY < 1e-6f)
    {
      if (z >= 0.f)
        return Pos::fromCartesian (0.f, 0.f, 0.f); // north pole

      // Exact south pole: mapTo3D() clamps theta at pi, so every r beyond
      // that point collapses onto this same 3D point — phi and the exact
      // original r are unrecoverable. Fall back to the smallest r that
      // reaches it, so re-grabbing resumes on the back of the sphere
      // instead of snapping to the front.
      auto const thetaMax = std::clamp (coverage, 0.05f, 1.0f) * pi<float> ();
      auto const rAtPole = pi<float> () / thetaMax;
      return Pos::fromCartesian (rAtPole, 0.f, 0.f);
    }

  // Colatitude recovered from the full 3D point — unlike sin(theta) (the
  // on-screen radius), atan2(rXY, z) is unambiguous between the front
  // (z >= 0) and back (z < 0) hemisphere.
  auto const theta = std::atan2 (rXY, z);
  auto const phi = std::atan2 (y, x);

  // Exact inverse of the piecewise theta(r) mapping in mapTo3D().
  auto const r = rFromThetaShape (theta, coverage);

  return Pos::fromCartesian (r * std::cos (phi), r * std::sin (phi), 0.f);
}

Pos
HeightMapSphere::mapTo2D (Pos const &pos3D, ElevationParams const &params) const
{
  auto const x = pos3D.x ();
  auto const y = pos3D.y ();
  auto const z = pos3D.z ();
  auto const phi = std::atan2 (y, x);

  if (params.flat)
    {
      // Flat mode carries no radius information in its elevation — every
      // point along the trajectory sits at the same fixed height. Fall
      // back to a nominal mid radius, same spirit as the coverage
      // overload's south-pole fallback.
      return Pos::fromCartesian (0.5f * std::cos (phi), 0.5f * std::sin (phi),
                                 0.f);
    }

  auto const rXY = std::sqrt (x * x + y * y);
  auto theta = std::atan2 (rXY, z);
  if (params.mirrorSouth)
    theta = pi<float> () - theta;

  auto const r
      = rFromThetaShape (theta, params.reach) * kPatternCoordinateMaxRadius;

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
