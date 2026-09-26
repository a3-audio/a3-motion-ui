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

#include <a3-motion-ui/components/LineMapStrokes.hh>

#include <vector>

namespace a3
{

/** One corner of the quad a GPU pass draws around one segment of a stroke.
 *
 *  The quad only has to be big enough; the shape comes from the fragment
 *  shader, which keeps what lies within `halfWidth` of the segment from `a`
 *  to `b` and fades the last texel for the edge. A run of these capsules is
 *  a stroke with curved joins and rounded ends -- the shape
 *  juce::PathStrokeType drew when these maps were stroked in software
 *  (a3-motion-ui#34).
 *
 *  Everything is in map texels, image orientation (y down), like MapStroke.
 */
struct CapsuleVertex
{
  float x, y;         ///< this corner
  float ax, ay;       ///< the segment's start
  float bx, by;       ///< and its end
  float halfWidth;
  float r, g, b;      ///< the stroke's colour, opaque
};

/** Six corners (two triangles) per segment, in the strokes' painting order:
 *  a GPU keeps submission order, and the order is what the cone is. */
std::vector<CapsuleVertex> capsuleVertices (std::vector<MapStroke> const &strokes);

}
