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

#include <algorithm>
#include <cmath>
#include <utility>

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
// the base's own latitude.
//
// It used to be a tenth whatever the run was, and the run is as long as the
// base is far from the pole -- half the sphere at ear height. A tenth of the
// pad crossing half the sphere is not a movement, it is a needle: drawn, four
// straight arms across the room that are in no shape, and a pass that missed
// the middle by a hair turned back halfway along one of them and ended in
// open air. That dangling end is what reads as a hole in the trajectory.
//
// So the tenth is the floor and a third is the ceiling, and in between the
// run gets pad in proportion to how far it has to go. Twice the run, because
// that is what brings the gradient within a factor of about three of the
// figure's own instead of ten, and because a near miss then turns back close
// enough to the pole to read as a figure's middle rather than as a cut line.
//
// The third is a real price and is why it is capped: a third of the pad's
// radius is a third of every figure's middle, funnelled towards a pole
// instead of standing on the base's latitude. It shows most with little
// reach, where there is not much figure for it to be a third of.
constexpr float kOriginFoldMin = 0.10f;
constexpr float kOriginFoldMax = 1.f / 3.f;

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

// Where the pad's middle runs to, and how much pad it is given to run in.
//
// One function because the forward map and its inverse both have to know, and
// a recording is written through the inverse and played back through the
// forward one -- two answers that drift apart do not fail, they quietly move
// every take.
struct OriginFold
{
  float radius; ///< normalised pad radius the run occupies
  float edge;   ///< the latitude it starts from, at that radius
  float pole;   ///< and the one it ends on, 0 north or 1 south
};

OriginFold
originFold (float base, float reach)
{
  auto const run = std::min (base, 1.f - base);
  auto const radius = std::clamp (2.f * run, kOriginFoldMin, kOriginFoldMax);

  auto const towards = reach < 0.f ? -1.f : 1.f;
  auto const theta
      = std::min (thetaShapeFromR (radius, std::abs (reach)), pi<float> ());
  auto const past = base + towards * theta / pi<float> ();
  auto const edge = past > 1.f ? 2.f - past : (past < 0.f ? -past : past);

  return { radius, edge, edge <= 0.5f ? 0.f : 1.f };
}

// What runs past a pole comes back on the far side rather than piling onto it.
float
overTheWall (float past)
{
  if (past > 1.f)
    return 2.f - past;
  if (past < 0.f)
    return -past;
  return past;
}

// The clip-top/-bottom clamp as a closed range of colatitudes.
std::pair<float, float>
clipRange (ElevationParams const &params)
{
  auto const rangeLow = std::clamp (params.clipTop, 0.f, 1.f);
  auto const rangeHigh = 1.f - std::clamp (params.clipBottom, 0.f, 1.f);
  if (rangeLow >= rangeHigh)
    {
      auto const middle = (rangeLow + rangeHigh) * 0.5f;
      return { middle, middle };
    }
  return { rangeLow, rangeHigh };
}

// Everything the forward map can reach for one clip (not flat), in the terms
// its inverse needs.
//
// The pad's radius runs the colatitude continuously: from the pole the run
// from the middle starts at, out to the run's edge, then along the band --
// "past", the base plus the shape's theta, unfolded -- until theta stops at
// pi. A continuous path over one interval reaches one interval of
// colatitudes, so what a clip can play is a band, [lowest, highest], and
// every direction outside it has a nearest one inside it: the same azimuth at
// the band's nearer edge (#66).
struct PlayableBand
{
  float base;
  float towards;
  OriginFold fold;
  float pastFrom; ///< the band's unfolded start, at the run's edge
  float pastTo;   ///< and its end, at theta = pi
  float lowest;   ///< what can be played, before the clip-top/-bottom clamp
  float highest;
};

PlayableBand
playableBand (ElevationParams const &params)
{
  PlayableBand band;
  band.base = std::clamp (params.elevationBase, 0.f, 1.f);
  band.towards = params.reach < 0.f ? -1.f : 1.f;
  band.fold = originFold (band.base, params.reach);

  auto const thetaAtEdge = std::min (
      thetaShapeFromR (band.fold.radius, std::abs (params.reach)), pi<float> ());
  band.pastFrom = band.base + band.towards * thetaAtEdge / pi<float> ();
  band.pastTo = band.base + band.towards;

  auto const pastLow = std::min (band.pastFrom, band.pastTo);
  auto const pastHigh = std::max (band.pastFrom, band.pastTo);

  band.lowest = std::min ({ band.fold.pole, overTheWall (band.pastFrom),
                            overTheWall (band.pastTo) });
  band.highest = std::max ({ band.fold.pole, overTheWall (band.pastFrom),
                             overTheWall (band.pastTo) });
  if (pastLow < 0.f && pastHigh > 0.f)
    band.lowest = 0.f;
  if (pastLow < 1.f && pastHigh > 1.f)
    band.highest = 1.f;
  return band;
}

// The normalised pad radius that plays `frac`, which has to be in the band.
//
// The band first, straight and then over the wall -- the outer pad is where a
// finger records, and what the inverse has always answered with. The run from
// the middle only for what the band does not reach: it covers latitudes on
// the pole's side of the run's edge, which the band may never come back to
// (a base of 0.1 reaches 0.05 only through it).
float
radiusPlaying (float frac, PlayableBand const &band, float reach)
{
  constexpr float slack = 1e-5f;
  auto const pastLow = std::min (band.pastFrom, band.pastTo) - slack;
  auto const pastHigh = std::max (band.pastFrom, band.pastTo) + slack;

  auto const wrapped = band.towards > 0.f ? 2.f - frac : -frac;
  for (auto const past : { frac, wrapped })
    if (past >= pastLow && past <= pastHigh)
      {
        auto const theta
            = std::clamp (band.towards * (past - band.base), 0.f, 1.f)
              * pi<float> ();
        return rFromThetaShape (theta, std::abs (reach));
      }

  auto const span = band.fold.edge - band.fold.pole;
  if (std::abs (span) < 1e-6f)
    return band.fold.radius;
  return band.fold.radius
         * std::clamp ((frac - band.fold.pole) / span, 0.f, 1.f);
}

}

float
elevationBaseCentring (float height, float reach)
{
  // The ring runs from the edge of the middle run to the drawn circle (pad
  // radius 1, i.e. 1/sqrt(2) of the pattern range). Its offsets below the
  // base do not depend on the base, but the run's width does -- twice the
  // base's distance to the nearer pole, clamped -- so a second pass settles
  // it; from the middle of the room the width is the cap anyway.
  auto const towards = reach < 0.f ? -1.f : 1.f;
  auto const spread = std::abs (reach);
  auto const ringOut
      = std::min (thetaShapeFromR (1.f / kPatternCoordinateMaxRadius, spread),
                  pi<float> ())
        / pi<float> ();

  auto base = std::clamp (height, 0.f, 1.f);
  for (int pass = 0; pass < 2; ++pass)
    {
      auto const fold = originFold (base, reach);
      auto const ringIn
          = std::min (thetaShapeFromR (fold.radius, spread), pi<float> ())
            / pi<float> ();
      base = std::clamp (height - towards * (ringIn + ringOut) * 0.5f, 0.f,
                         1.f);
    }
  return base;
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

    // Over the wall at whichever end it reaches: what runs past a pole comes
    // back on the far side rather than piling onto it.
    auto const heightAt
        = [&] (float t) { return overTheWall (base + towards * t / pi<float> ()); };

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
    // To the nearer of the two poles, and over as much pad as the run needs
    // -- see originFold(), which is where both of those are decided and why.
    auto const fold = originFold (base, params.reach);

    if (rNorm < fold.radius)
      {
        return fold.pole
               + (fold.edge - fold.pole) * (rNorm / fold.radius);
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
  auto const [bandLow, bandHigh] = clipRange (params);

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

  // The exact inverse of the forward step wherever the clip can play, and the
  // nearest direction it can play everywhere else. A take is written through
  // here and played back through mapTo3D, and the engine puts the blob on
  // that round trip while recording -- so what is heard during the take is
  // what the take plays back (#66).
  //
  // It used to undo the base on the assumption that every direction was
  // reachable. With the base off the pole the band does not reach up to it,
  // and a finger above the band went the "over the floor and back up" branch
  // into a theta past pi, which mapTo3D clamps: every such finger played back
  // on one ring at 1 - base. A finger outside the band is now held at its
  // nearer edge, in its own azimuth, which is the nearest direction there is.
  auto const rXY = std::sqrt (x * x + y * y);
  auto const touched = std::atan2 (rXY, z) / pi<float> ();

  auto const band = playableBand (params);
  auto const [clipLow, clipHigh] = clipRange (params);

  // What can be heard is the band with the clips laid over it; the radius
  // asked for is one the band itself reaches, so the clip clamp in mapTo3D
  // lands it on exactly `heard`.
  auto const heard = std::clamp (
      touched, std::clamp (band.lowest, clipLow, clipHigh),
      std::clamp (band.highest, clipLow, clipHigh));
  auto const reached = std::clamp (heard, band.lowest, band.highest);

  auto const r = radiusPlaying (reached, band, params.reach)
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
