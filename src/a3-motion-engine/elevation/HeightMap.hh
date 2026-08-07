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

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** Per-clip elevation parameters — see HeightMap::mapTo3D()'s struct
 *  overload for the full semantics. Kept as a bundle (rather than separate
 *  arguments) since it has grown past what's comfortable as a positional
 *  parameter list; also lets Pattern hand it out as one value via
 *  Pattern::getElevationParams(). */
struct ElevationParams
{
  // 0.05..1.0: how far around the sphere the pattern's own coordinate
  // range reaches from the pole. 0.5 = hemisphere (its outer edge sits on
  // the equator), 1.0 = full sphere (opposite pole). r=0 (pad centre)
  // always maps to the pole itself. "Its own coordinate range" is the -1..1
  // SVG viewBox every pattern is authored in (corner distance sqrt(2)) —
  // see HeightMapSphere's kPatternCoordinateMaxRadius — not the touchpad's
  // visible-circle radius, so non-circular shapes (whose points commonly
  // exceed r=1, e.g. a Square's corners at r=sqrt(2)) scale proportionally
  // instead of overshooting past reach's edge.
  float reach = 0.5f;
  // false: the reach cone grows from the north pole (default). true: grows
  // from the south pole instead (mirrors the cone through the equator).
  bool mirrorSouth = false;
  // 0..1: absolute hard bound excluding this fraction of the range from
  // the north pole side. A plain final clamp — does not interact with
  // reach/mirrorSouth's shape.
  float clipTop = 0.0f;
  // 0..1: same as clipTop but excluding from the south pole side.
  float clipBottom = 0.0f;
  // If true, ignore reach/mirrorSouth entirely and use flatElevation for
  // every point on the trajectory (azimuth is still preserved).
  bool flat = false;
  // 0..1 (0 = north pole, 1 = south pole): fixed elevation used when
  // flat == true.
  float flatElevation = 0.5f;
};

class HeightMap
{
public:
  virtual ~HeightMap () = default;

  virtual float computeHeight (Pos const &pos) const = 0;

  /** Map a 2D position (x,y) onto the 3D sphere/surface.
   *  Default implementation preserves x,y and sets z = computeHeight().
   *  Subclasses can override for full spherical wrapping.
   *  Uses the stored (global) coverage value. */
  virtual Pos mapTo3D (Pos const &pos2D) const
  {
    return Pos::fromCartesian (
        pos2D.x (), pos2D.y (), computeHeight (pos2D));
  }

  /** Map a 2D position onto 3D using an explicit coverage value.
   *  This allows per-channel elevation without changing internal state. */
  virtual Pos mapTo3D (Pos const &pos2D, float coverage) const
  {
    (void) coverage;
    return mapTo3D (pos2D);
  }

  /** Map a 2D position onto 3D using an explicit per-clip ElevationParams
   *  set — allows per-clip elevation without touching any shared/global
   *  state. Strictly monotonic in r (unlike the old wrap/elevation/base-
   *  point scheme this replaced): r=0 is always the pole, r=1 is always
   *  `reach`'s point, nothing folds back on itself. See ElevationParams for
   *  the individual field semantics.
   *
   *  Default implementation ignores everything but `reach` and falls back
   *  to the 2-arg overload. */
  virtual Pos mapTo3D (Pos const &pos2D, ElevationParams const &params) const
  {
    return mapTo3D (pos2D, params.reach);
  }

  /** Exact inverse of mapTo3D(): recover the 2D disc position that was
   *  originally used to produce this 3D position. Unlike re-deriving a 2D
   *  position from the on-screen (orthographic) projection of pos3D — which
   *  is ambiguous, since sin(theta) is the same for a colatitude and its
   *  supplement — this uses the full 3D point (including z) and is
   *  therefore unambiguous between the front and back hemisphere. */
  virtual Pos mapTo2D (Pos const &pos3D) const
  {
    return Pos::fromCartesian (pos3D.x (), pos3D.y (), 0.f);
  }

  /** Same as mapTo2D(pos3D) but uses an explicit coverage value. */
  virtual Pos mapTo2D (Pos const &pos3D, float coverage) const
  {
    (void) coverage;
    return mapTo2D (pos3D);
  }

  /** Same as mapTo2D(pos3D, coverage) but with an explicit ElevationParams
   *  set too — see mapTo3D()'s struct overload. */
  virtual Pos mapTo2D (Pos const &pos3D, ElevationParams const &params) const
  {
    return mapTo2D (pos3D, params.reach);
  }

  /** Set the coverage parameter (0.0 – 1.0).
   *  0.0 = flat / top only, 1.0 = full sphere.
   *  Default implementation is a no-op. */
  virtual void setCoverage (float /*coverage*/) {}

  /** Get the current coverage parameter. Default: 0.5 (hemisphere). */
  virtual float getCoverage () const { return 0.5f; }
};

}
