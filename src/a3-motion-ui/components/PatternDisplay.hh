/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-ui/components/ElevationSideView.hh>

#include <utility>
#include <vector>

namespace a3
{

class Pattern;
class PatternLibrary;
class HeightMap;

/** What the sphere draws of a clip: a line, or -- for a shape made of dots,
 *  which has none -- its dots. */
struct PatternDisplaySource
{
  juce::Path path;
  std::vector<std::pair<float, float> > jumpDots;
};

/** One answer for every route that asks (2026-09-27): a shape of dots shows
 *  its dots from the library file, everything else the line its ticks run on,
 *  cut where it jumps. Two routes that each had half of this is how Cross lost
 *  its dots on the first knob turned. */
PatternDisplaySource patternDisplayFor (Pattern const &pattern,
                                        PatternLibrary const &library);

/** A clip in the elevation picture: the line it runs on, or -- for a shape
 *  of dots -- its dots, each put through the same shaping, swept elevation and
 *  height map the engine plays it through, and seen from the side the picture
 *  looks from. The line sampled down to at most `maxPoints`. */
struct ElevationFigure
{
  std::vector<ElevationSidePoint> line;
  std::vector<ElevationSidePoint> dots;
};

ElevationFigure elevationFigureFor (Pattern const &pattern,
                                    PatternLibrary const &library,
                                    HeightMap const &heightMap,
                                    SphereCamera camera, std::size_t maxPoints);

}
