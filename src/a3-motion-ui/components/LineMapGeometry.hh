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

#include <JuceHeader.h>

#include <a3-motion-engine/TrajectoryShaping.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-ui/components/SphereProjection.hh>

#include <vector>

namespace a3
{

/** A trajectory as the camera sees it on the sphere: the points every
 *  rasteriser of the line maps draws from.
 *
 *  Taken out of drawPathOnSphere() so the software strokes and the GPU pass
 *  (a3-motion-ui#34) draw one line, not two that happen to agree. It is the
 *  same projection the engine puts the blob through -- shaped, lifted by the
 *  height map, turned to the camera -- which is what keeps the blob on it.
 *
 *  The three vectors are always the same length.
 */
struct ProjectedLine
{
  /** On screen, in the sphere's normalised units (cartesian2DHOA2JUCE). */
  std::vector<juce::Point<float> > points;
  /** How far towards the viewer: +1 facing, -1 the far side. */
  std::vector<float> depth;
  /** True where the pen is lifted before this point: a new sub-path, a jump
   *  too long to be a movement, or a crossing of the disc's origin. */
  std::vector<bool> startsRun;
};

ProjectedLine projectLine (juce::Path const &displayPath,
                           ElevationParams const &elevationParams,
                           HeightMap const &heightMap,
                           PlaneShaping const &shaping,
                           SphereCamera const &camera);

}
