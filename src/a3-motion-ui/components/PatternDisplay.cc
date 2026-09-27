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

#include "PatternDisplay.hh"

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PatternLibrary.hh>
#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/TrajectoryShape.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>
#include <a3-motion-engine/elevation/HeightMap.hh>
#include <a3-motion-ui/Helpers.hh>

namespace a3
{

PatternDisplaySource
patternDisplayFor (Pattern const &pattern, PatternLibrary const &library)
{
  PatternDisplaySource shown;

  // A shape made of dots has no line to draw, and its dots are only in the
  // file: those come from the library.
  if (auto const index = library.indexForName (pattern.getName ()); index > 0)
    {
      auto const &entry = library.getEntry (index);
      if (entry.hasJumpDots && svgDToPath (entry.svgPathData).isEmpty ())
        {
          shown.jumpDots = entry.jumpDots;
          return shown;
        }
    }

  // Everything else from the ticks, because that is what plays -- cut at
  // teleports as well as at gaps, the way the take's own trail is drawn, so
  // a jump the clip still has is not bridged by a line.
  for (auto const &segment : trajectorySegments (pattern.getTicks ().positions,
                                                 pattern.getBridgePlan ()))
    {
      shown.path.startNewSubPath (segment.front ().x (),
                                  segment.front ().y ());
      for (size_t i = 1; i < segment.size (); ++i)
        shown.path.lineTo (segment[i].x (), segment[i].y ());
    }
  return shown;
}

ElevationFigure
elevationFigureFor (Pattern const &pattern, PatternLibrary const &library,
                    HeightMap const &heightMap, SphereCamera camera,
                    std::size_t maxPoints)
{
  // Through exactly the calls the engine plays it through (performPlayback):
  // the same shaping, the same swept elevation, the same height map.
  auto const shaping = shapingOf (pattern);
  auto const params = sweptElevation (pattern.getElevationParams (), pattern);
  auto const turn = spaceTurnOf (pattern);
  auto const onSphere = [&] (Pos const &recorded) {
    return turnedInSpace (
        heightMap.mapTo3D (shapedPosition (recorded, shaping), params), turn);
  };

  ElevationFigure figure;

  auto const shown = patternDisplayFor (pattern, library);
  if (!shown.jumpDots.empty ())
    {
      for (auto const &[x, y] : shown.jumpDots)
        figure.dots.push_back (
            elevationSideView (onSphere (Pos::fromCartesian (x, y, 0.f)),
                               camera));
      return figure;
    }

  auto const ticks = pattern.getTicks ().positions;
  std::vector<Pos> directions;
  directions.reserve (ticks.size ());
  for (auto const &tick : ticks)
    directions.push_back (tick.isValid () ? onSphere (tick) : Pos::invalid);

  figure.line = elevationSideView (directions, maxPoints, camera);
  return figure;
}

std::vector<DrawnClip>
clipsToDraw (ClipGrid const &running, ClipGrid const &filled,
             index_t selectedChannel, index_t selectedSlot)
{
  std::vector<DrawnClip> drawn;

  for (index_t channel = 0; channel < running.size (); ++channel)
    for (index_t slot = 0; slot < running[channel].size (); ++slot)
      if (running[channel][slot]
          && !(channel == selectedChannel && slot == selectedSlot))
        drawn.push_back ({ channel, slot, false });

  if (selectedChannel < filled.size ()
      && selectedSlot < filled[selectedChannel].size ()
      && filled[selectedChannel][selectedSlot])
    drawn.push_back ({ selectedChannel, selectedSlot, true });

  return drawn;
}

}
