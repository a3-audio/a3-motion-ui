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

  // The pad is *wrapped around* the base, not sheared towards it.
  //
  // It used to take the disc's angle as the room's azimuth and the disc's
  // radius as a change in colatitude. That is a proper wrapping only while
  // the base is a pole: anywhere else the pad's centre stands for "this
  // colatitude, any azimuth" -- a whole circle of directions -- so two
  // neighbouring ticks either side of the centre landed on opposite sides of
  // it. Measured on a Clover: at a base of 0 the largest step between two
  // ticks was the average one; at 0.25 it was 117 times it. The sound
  // teleported four times a lap, and the drawn line was torn where it did.
  //
  // So: the radius is the angular distance from the base *direction*, and the
  // disc's angle is the bearing around it. At a base of 0 that is the same
  // arithmetic as before, which is why every clip sitting at the pole sounds
  // exactly as it did.
  auto const direction = [&] {
    if (params.flat)
      {
        // Flat has no radius to travel: every point sits at one colatitude,
        // and the disc's angle is the azimuth outright.
        auto const flat = std::clamp (params.flatElevation, 0.f, 1.f)
                          * pi<float> ();
        return Pos::fromCartesian (std::sin (flat) * std::cos (phi),
                                   std::sin (flat) * std::sin (phi),
                                   std::cos (flat));
      }

    auto const r = std::sqrt (x * x + y * y);
    auto const rNorm = r / kPatternCoordinateMaxRadius;
    auto const theta
        = std::min (thetaShapeFromR (rNorm, params.reach), pi<float> ());

    // The base direction, and the two tangents at it: south along the
    // meridian, and east. A point of the pad is theta away from the base,
    // on the bearing the pad's own angle names.
    auto const b = std::clamp (params.elevationBase, 0.f, 1.f) * pi<float> ();
    auto const sinB = std::sin (b);
    auto const cosB = std::cos (b);

    auto const c = std::cos (theta);
    auto const s = std::sin (theta);
    auto const towardsSouth = s * std::cos (phi);
    auto const towardsEast = s * std::sin (phi);

    return Pos::fromCartesian (c * sinB + towardsSouth * cosB, towardsEast,
                               c * cosB - towardsSouth * sinB);
  }();

  // clipTop/clipBottom are a plain, final, absolute clamp — [bandLow,
  // bandHigh] as a fraction of the full north(0)-to-south(1) range. A point
  // pushed past one of them slides *along* it: its bearing is kept and only
  // its colatitude is pulled back, so a figure that runs into the ceiling
  // travels along the ceiling rather than piling onto a single spot.
  auto const rangeLow = std::clamp (params.clipTop, 0.f, 1.f);
  auto const rangeHigh = 1.f - std::clamp (params.clipBottom, 0.f, 1.f);
  bool const collapsed = rangeLow >= rangeHigh;
  auto const bandLow = collapsed ? (rangeLow + rangeHigh) * 0.5f
                                 : std::min (rangeLow, rangeHigh);
  auto const bandHigh = collapsed ? bandLow : std::max (rangeLow, rangeHigh);

  auto const frac = std::atan2 (std::sqrt (direction.x () * direction.x ()
                                           + direction.y () * direction.y ()),
                                direction.z ())
                    / pi<float> ();
  auto const held = std::clamp (frac, bandLow, bandHigh);

  if (std::abs (held - frac) < 1e-6f)
    return direction;

  auto const bearing = std::atan2 (direction.y (), direction.x ());
  auto const thetaFinal = held * pi<float> ();

  return Pos::fromCartesian (std::sin (thetaFinal) * std::cos (bearing),
                             std::sin (thetaFinal) * std::sin (bearing),
                             std::cos (thetaFinal));
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

  // The exact inverse of the forward step: how far this direction is from the
  // base, and on what bearing around it. Getting this wrong does not show as
  // an error -- a recording is written through here and played back through
  // mapTo3D, so the take would simply sit somewhere else.
  auto const b = std::clamp (params.elevationBase, 0.f, 1.f) * pi<float> ();
  auto const sinB = std::sin (b);
  auto const cosB = std::cos (b);

  // Into the base's own frame: how far along the meridian, how far east, and
  // how much of the way towards the base itself.
  auto const towardsBase = x * sinB + z * cosB;
  auto const towardsSouth = x * cosB - z * sinB;
  auto const towardsEast = y;

  auto const theta
      = std::atan2 (std::sqrt (towardsSouth * towardsSouth
                               + towardsEast * towardsEast),
                    towardsBase);
  auto const bearing = std::atan2 (towardsEast, towardsSouth);

  auto const r
      = rFromThetaShape (theta, params.reach) * kPatternCoordinateMaxRadius;

  return Pos::fromCartesian (r * std::cos (bearing), r * std::sin (bearing),
                             0.f);
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
