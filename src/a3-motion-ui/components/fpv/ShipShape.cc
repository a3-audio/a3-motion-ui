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

#include "ShipShape.hh"

#include <cmath>

namespace a3
{

void
ShipHeading::update (juce::Point<float> at, float minimumStep)
{
  if (_last && _last->getDistanceFrom (at) < minimumStep)
    return;
  if (_last)
    {
      auto const d = at - *_last;
      // Screen y grows downward, so "up" is -y: 0 = up, pi/2 = right.
      _radians = std::atan2 (d.x, -d.y);
    }
  _last = at;
}

void
ShipHeading::lose ()
{
  _last.reset ();
}

float
ShipHeading::radians () const
{
  return _radians;
}

juce::Path
shipPath (juce::Point<float> centre, float radians, float length)
{
  auto const half = length / 2.f;
  auto const halfWidth = length * shipWidthOfLength / 2.f;
  juce::Path p;
  p.startNewSubPath (0.f, -half);                    // nose
  p.lineTo (halfWidth, half);                        // right tail
  p.lineTo (0.f, half - length * shipNotchOfLength); // notch
  p.lineTo (-halfWidth, half);                       // left tail
  p.closeSubPath ();
  p.applyTransform (
      juce::AffineTransform::rotation (radians).translated (centre));
  return p;
}

}
