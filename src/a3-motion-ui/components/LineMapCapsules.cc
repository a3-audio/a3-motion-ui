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

#include <a3-motion-ui/components/LineMapCapsules.hh>

namespace a3
{

namespace
{

// Past half the width by the one texel the shader fades for the edge -- the
// quad only has to be big enough, and a texel short is a clipped stroke.
constexpr float edgeTexels = 1.f;

void
addSegment (std::vector<CapsuleVertex> &out, juce::Point<float> a,
            juce::Point<float> b, float halfWidth, juce::Colour colour)
{
  auto const reach = halfWidth + edgeTexels;

  // Along the segment and across it. A segment of no length has no direction
  // of its own and is a round dot; any direction will do for its quad.
  auto along = b - a;
  auto const length = along.getDistanceFromOrigin ();
  along = length > 0.f ? along / length : juce::Point<float> (1.f, 0.f);
  auto const across = juce::Point<float> (-along.y, along.x);

  auto const corner = [&] (juce::Point<float> end, float forward, float side) {
    auto const at = end + along * (forward * reach) + across * (side * reach);
    return CapsuleVertex{ at.x,
                          at.y,
                          a.x,
                          a.y,
                          b.x,
                          b.y,
                          halfWidth,
                          colour.getFloatRed (),
                          colour.getFloatGreen (),
                          colour.getFloatBlue () };
  };

  auto const startLeft = corner (a, -1.f, 1.f);
  auto const startRight = corner (a, -1.f, -1.f);
  auto const endLeft = corner (b, 1.f, 1.f);
  auto const endRight = corner (b, 1.f, -1.f);

  out.push_back (startLeft);
  out.push_back (startRight);
  out.push_back (endLeft);
  out.push_back (endLeft);
  out.push_back (startRight);
  out.push_back (endRight);
}

}

std::vector<CapsuleVertex>
capsuleVertices (std::vector<MapStroke> const &strokes)
{
  std::vector<CapsuleVertex> out;
  for (auto const &stroke : strokes)
    for (std::size_t i = 1; i < stroke.points.size (); ++i)
      if (!stroke.lifts[i])
        addSegment (out, stroke.points[i - 1], stroke.points[i],
                    stroke.width * 0.5f, stroke.colour);
  return out;
}

}
