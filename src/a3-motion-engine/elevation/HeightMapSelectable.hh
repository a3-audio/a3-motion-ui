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
#include <a3-motion-engine/elevation/HeightMapFlat.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

namespace a3
{

/** Runtime-selectable HeightMap: owns one instance of each concrete
 *  strategy and delegates every call to whichever is currently active.
 *  Lets the UI switch elevation mapping live (e.g. from the overlay menu)
 *  without MotionEngine needing to know about the change, since it only
 *  ever holds a HeightMap reference. */
class HeightMapSelectable : public HeightMap
{
public:
  enum class Strategy { SphereWrap, SphereClamped, Flat };

  void setStrategy (Strategy strategy);
  Strategy getStrategy () const;

  float computeHeight (Pos const &pos) const override;

  Pos mapTo3D (Pos const &pos2D) const override;
  Pos mapTo3D (Pos const &pos2D, float coverage) const override;

  Pos mapTo2D (Pos const &pos3D) const override;
  Pos mapTo2D (Pos const &pos3D, float coverage) const override;

  void setCoverage (float coverage) override;
  float getCoverage () const override;

private:
  HeightMap const &activeDelegate () const;

  HeightMapSphere _sphere;
  HeightMapFlat _flat;
  std::atomic<Strategy> _strategy{ Strategy::SphereWrap };
};

}
