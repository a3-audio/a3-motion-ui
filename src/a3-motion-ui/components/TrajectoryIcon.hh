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

#include <a3-motion-engine/util/Types.hh>

#include <vector>

namespace a3
{

/**
 * Small pictogram of a recorded/loaded trajectory: either a continuous
 * stroked path (HOA-normalised [-1,1] space) or a set of jump dots, whatever
 * best represents the pattern's shape. Shared between PadRowDisplay and
 * ClipSettingsComponent so both render icons identically.
 */
struct TrajectoryIconData
{
  juce::Path path; ///< normalised [-1,1], empty if hasJumpDots or !hasIcon
  std::vector<std::pair<float, float>> jumpDots; ///< normalised [-1,1]
  bool hasJumpDots = false;
  bool hasIcon = false; ///< false = nothing to draw (e.g. empty pattern slot)
};

/** Build icon data from a pre-normalised SVG-derived path (e.g. from
 *  PatternLibrary::Entry::svgPathData via svgDToPath()). */
TrajectoryIconData trajectoryIconFromPath (
    juce::Path const &path,
    std::vector<std::pair<float, float>> const &jumpDots);

/** Build icon data from raw XY tick data: finds the bounding box, downsamples
 *  to a manageable point count, and either fits a Catmull-Rom spline through
 *  the points or (if most ticks are invalid/jumps) collects distinct jump
 *  points as dots. */
TrajectoryIconData trajectoryIconFromTicks (std::vector<Pos> const &ticks);

/** Draw previously built icon data into `area`, stroked/filled in `colour`.
 *  No-op if `data.hasIcon` is false. */
/** `turns` is the standing rotation the clip is set to, in turns clockwise --
 *  the picture is of this clip, and this clip is turned. The spin is
 *  deliberately not added: it is a movement, and a picture that moved would be
 *  a second thing to watch beside the sphere, which is where movement belongs.
 *  The spin says what it is doing on the rotate knob's blue arc instead. */
void drawTrajectoryIcon (juce::Graphics &g, juce::Rectangle<float> area,
                         TrajectoryIconData const &data, juce::Colour colour,
                         float turns = 0.f);

}
