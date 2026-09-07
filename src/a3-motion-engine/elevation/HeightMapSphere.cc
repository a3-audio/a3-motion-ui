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

// How much of the pad, measured from its middle outwards in the normalised
// radius the shape formula works in, runs to the pole instead of standing on
// the base's own latitude. A tenth: small enough that a figure keeps its
// shape, large enough that the run is spread over enough ticks to be a
// movement rather than a jump.
constexpr float kOriginFold = 0.10f;

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

  // The pad is a band: the disc's angle is the room's azimuth and its radius
  // is how far *down* from the base the point sits. So raising the base slides
  // the whole figure down and what reaches the equator carries on over it onto
  // the far side -- the figure wraps over the outer wall rather than moving
  // sideways, which is what an elevation control should do.
  //
  // The cost, and it is a real one: the pad's centre has no azimuth of its own
  // in this model, so it stands for a whole circle of directions once the base
  // is off the pole, and a path crossing the centre is torn there. That is
  // topology, not a bug to find -- a map that keeps the figure centred on the
  // room's axis while its middle sits off the pole cannot be continuous at
  // that middle. Shapes that cross the pad's centre exactly: Clover, Infinity,
  // Rose 4-Petal, and any take driven through the middle of the pad.
  //
  // What was tried and rejected: wrapping the pad around the base direction as
  // a cap. That is continuous everywhere, and it moves the figure's centre
  // across the room, which reads as the view tilting and is not what elevation
  // means here.
  //
  // The cone always grows the same way -- south, down and over the wall. It
  // used to grow towards whichever pole was further away, which is a rule with
  // a jump in it at a base of exactly one half, right where sway spends its
  // time.
  //
  auto const frac = [&] {
    if (params.flat)
      return std::clamp (params.flatElevation, 0.f, 1.f);

    auto const r = std::sqrt (x * x + y * y);
    auto const rNorm = r / kPatternCoordinateMaxRadius;
    auto const theta = std::min (
        thetaShapeFromR (rNorm, std::abs (params.reach)), pi<float> ());

    auto const base = std::clamp (params.elevationBase, 0.f, 1.f);

    // The size of the reach is how far the figure spreads and its sign is
    // which way. Down was the only way once the cone stopped choosing a pole
    // for itself, which left a figure sitting at the ceiling with nowhere to
    // be put -- the base is at the top and reach is the only thing that says
    // where the rest of it goes.
    auto const towards = params.reach < 0.f ? -1.f : 1.f;

    auto const heightAt = [&] (float t) {
      auto const past = base + towards * t / pi<float> ();

      // Over the wall at whichever end it reaches: what runs past a pole comes
      // back on the far side rather than piling onto it.
      if (past > 1.f)
        return 2.f - past;
      if (past < 0.f)
        return -past;
      return past;
    };

    // The middle of the pad runs to the pole rather than standing on the
    // base's own latitude.
    //
    // One point of the pad standing for a whole circle of the room is what
    // tore a figure crossing the middle: two neighbouring ticks either side of
    // it landed on opposite sides of a latitude, and the sound jumped -- a
    // hundred and seventeen times the ordinary step at a base of a quarter.
    // A circle can only be closed up continuously by filling what it bounds,
    // and the one thing a latitude bounds is a cap. So the innermost tenth of
    // the pad *is* that cap: the pole at the very middle, the latitude the
    // rest of the figure stands on at the tenth, and every height between them
    // on the way. Crossing the middle is a run to the ceiling and back rather
    // than a jump across the room, and every point of it is a place the sound
    // actually goes.
    //
    // Towards the ceiling for a figure that grows downwards and the floor for
    // one that grows up -- away from the rest of itself, so the run is into
    // room the figure is not already using.
    if (rNorm < kOriginFold)
      {
        auto const edge = heightAt (std::min (
            thetaShapeFromR (kOriginFold, std::abs (params.reach)),
            pi<float> ()));
        // The pole on the far side from where the rest of the figure goes, so
        // the run is into room the figure is not already using. Read off where
        // the figure actually is rather than off the sign of the reach: with
        // the base at a pole the figure has wrapped, and then a reach that
        // says "down" is going up.
        auto const pole = edge >= base ? 0.f : 1.f;

        return pole + (edge - pole) * (rNorm / kOriginFold);
      }

    return heightAt (theta);
  }();

  auto const direction = [&] {
    auto const t = frac * pi<float> ();
    return Pos::fromCartesian (std::sin (t) * std::cos (phi),
                               std::sin (t) * std::sin (phi), std::cos (t));
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

  // The exact inverse of the forward step: undo the base, and what is left is
  // the theta the shape was built from. Getting this wrong does not show as an
  // error -- a recording is written through here and played back through
  // mapTo3D, so the take would simply sit somewhere else.
  auto const rXY = std::sqrt (x * x + y * y);
  auto const frac = std::atan2 (rXY, z) / pi<float> ();

  auto const base = std::clamp (params.elevationBase, 0.f, 1.f);

  // Undone the way it was done: down from the base, or up from it, and what
  // went past a pole came back on the far side, so that is the branch to
  // recognise. Written out rather than folded into one expression -- a
  // recording is written through here and played back through mapTo3D, and a
  // clever inverse that is wrong moves every take instead of failing.
  auto const away = [&] {
    if (params.reach < 0.f)
      return frac <= base ? base - frac  // straight up from the base
                          : base + frac; // over the ceiling and back down
    return frac >= base ? frac - base    // straight down from it
                        : 2.f - base - frac; // over the floor and back up
  }();

  auto const theta = away * pi<float> ();

  auto const r = rFromThetaShape (theta, std::abs (params.reach))
                 * kPatternCoordinateMaxRadius;

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
