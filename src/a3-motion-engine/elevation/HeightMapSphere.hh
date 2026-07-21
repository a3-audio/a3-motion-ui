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

#include <atomic>

#include <a3-motion-engine/elevation/HeightMap.hh>

namespace a3
{

class HeightMapSphere : public HeightMap
{
public:
  /** Controls what happens when a 2D position is dragged past the visible
   *  disc (r > 1): Wrap keeps growing colatitude past thetaMax so the
   *  position continues under the sphere (default, original behaviour).
   *  Clamp freezes colatitude at thetaMax, i.e. the position stays pinned
   *  to the sphere's edge instead of wrapping underneath. */
  enum class EdgeMode { Wrap, Clamp };

  void setEdgeMode (EdgeMode mode);
  EdgeMode getEdgeMode () const;

  float computeHeight (Pos const &pos) const override;

  /** Map 2D disc position onto the sphere surface.
   *  Coverage controls how far around the sphere the disc wraps:
   *    coverage = 0.5  → hemisphere (north pole to equator)
   *    coverage = 1.0  → full sphere (north pole to south pole)
   *    coverage = 0.33 → top third (~60° colatitude)
   *  The mapping preserves the azimuth angle and remaps the radial
   *  distance to colatitude: θ = (r / r_max) × θ_max. */
  Pos mapTo3D (Pos const &pos2D) const override;

  /** Map 2D disc position onto sphere using an explicit coverage value.
   *  Same as mapTo3D(pos2D) but uses the given coverage instead of
   *  the internal stored value. Enables per-channel elevation. */
  Pos mapTo3D (Pos const &pos2D, float coverage) const override;

  /** Exact inverse of mapTo3D(): recovers the 2D disc position from a full
   *  3D point, unambiguous between front and back hemisphere (see
   *  HeightMap::mapTo2D). */
  Pos mapTo2D (Pos const &pos3D) const override;
  Pos mapTo2D (Pos const &pos3D, float coverage) const override;

  void setCoverage (float coverage) override;
  float getCoverage () const override;

private:
  std::atomic<float> _coverage{ 0.5f };  // 0.5 = hemisphere (default)
  std::atomic<EdgeMode> _edgeMode{ EdgeMode::Wrap };
};

}
