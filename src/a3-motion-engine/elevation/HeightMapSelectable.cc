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

#include "HeightMapSelectable.hh"

namespace a3
{

void
HeightMapSelectable::setStrategy (Strategy strategy)
{
  if (strategy == Strategy::SphereClamped)
    _sphere.setEdgeMode (HeightMapSphere::EdgeMode::Clamp);
  else if (strategy == Strategy::SphereWrap)
    _sphere.setEdgeMode (HeightMapSphere::EdgeMode::Wrap);

  _strategy.store (strategy, std::memory_order_relaxed);
}

HeightMapSelectable::Strategy
HeightMapSelectable::getStrategy () const
{
  return _strategy.load (std::memory_order_relaxed);
}

HeightMap const &
HeightMapSelectable::activeDelegate () const
{
  if (_strategy.load (std::memory_order_relaxed) == Strategy::Flat)
    return _flat;
  return _sphere;
}

float
HeightMapSelectable::computeHeight (Pos const &pos) const
{
  return activeDelegate ().computeHeight (pos);
}

Pos
HeightMapSelectable::mapTo3D (Pos const &pos2D) const
{
  return activeDelegate ().mapTo3D (pos2D);
}

Pos
HeightMapSelectable::mapTo3D (Pos const &pos2D, float coverage) const
{
  return activeDelegate ().mapTo3D (pos2D, coverage);
}

Pos
HeightMapSelectable::mapTo2D (Pos const &pos3D) const
{
  return activeDelegate ().mapTo2D (pos3D);
}

Pos
HeightMapSelectable::mapTo2D (Pos const &pos3D, float coverage) const
{
  return activeDelegate ().mapTo2D (pos3D, coverage);
}

void
HeightMapSelectable::setCoverage (float coverage)
{
  _sphere.setCoverage (coverage);
}

float
HeightMapSelectable::getCoverage () const
{
  return _sphere.getCoverage ();
}

}
