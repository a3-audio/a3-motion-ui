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

  /** Set the coverage parameter (0.0 – 1.0).
   *  0.0 = flat / top only, 1.0 = full sphere.
   *  Default implementation is a no-op. */
  virtual void setCoverage (float /*coverage*/) {}

  /** Get the current coverage parameter. Default: 0.5 (hemisphere). */
  virtual float getCoverage () const { return 0.5f; }
};

}
