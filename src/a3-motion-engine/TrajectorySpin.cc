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

#include "TrajectorySpin.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

Pos
spinPosition (Pos const &position, float phase)
{
  // Negated, and this is the one place the direction is decided. Turning the
  // control to the right has to turn the trajectory to the right, and the
  // screen is a mirror of these coordinates: cartesian2DHOA2JUCE (Helpers.hh)
  // puts an HOA position at { -y, -x }, so a mathematically positive turn —
  // x towards y — walks the blob from the top of the sphere to its left,
  // which reads as anticlockwise.
  auto const angle = -phase * juce::MathConstants<float>::twoPi;
  auto const c = std::cos (angle);
  auto const s = std::sin (angle);

  auto const x = position.x ();
  auto const y = position.y ();

  return Pos::fromCartesian (x * c - y * s, x * s + y * c, position.z ());
}

}
