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

#include "TrajectoryShaping.hh"

#include <algorithm>
#include <cmath>

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TrajectorySpin.hh>

namespace a3
{

float
squeezeFactor (float amount)
{
  return std::pow (2.f, std::clamp (amount, -1.f, 1.f));
}

PlaneShaping
shapingOf (Pattern const &pattern)
{
  PlaneShaping shaping;

  shaping.turns = pattern.getRotate () + pattern.getSpinPhase ();
  shaping.squeezeX = pattern.getSqueezeX ();
  shaping.squeezeY = pattern.getSqueezeY ();

  return shaping;
}

Pos
shapedPosition (Pos const &recorded, PlaneShaping const &shaping)
{
  // No sign to arrange here, unlike spinPosition(): the screen mirrors these
  // coordinates, and a mirror leaves a scaling about the origin alone.
  auto const squeezed = Pos::fromCartesian (
      recorded.x () * squeezeFactor (shaping.squeezeX),
      recorded.y () * squeezeFactor (shaping.squeezeY), recorded.z ());

  return spinPosition (squeezed, shaping.turns);
}

}
