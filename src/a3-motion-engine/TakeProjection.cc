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

#include "TakeProjection.hh"

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>

namespace a3
{

Pos
playedPosition (HeightMap const &heightMap, Pos const &position2D,
                Pattern const &pattern)
{
  if (!position2D.isValid ())
    return position2D;

  // Shaped before it is projected: in the 2D disc the radius is the
  // elevation and the angle the azimuth, so turning the disc turns the
  // trajectory around the pole and squeezing an axis presses the figure flat
  // without moving where it sits. The renderer makes the same calls --
  // shapedPosition(), sweptElevation(), spaceTurnOf() -- from the same
  // phases, or the line would be drawn somewhere the blob is not running.
  auto const shaped = shapedPosition (position2D, shapingOf (pattern));
  auto const params = sweptElevation (pattern.getElevationParams (), pattern);
  return turnedInSpace (heightMap.mapTo3D (shaped, params),
                        spaceTurnOf (pattern));
}

Pos
writtenPosition (HeightMap const &heightMap, Pos const &direction,
                 Pattern const &pattern)
{
  if (!direction.isValid ())
    return direction;

  auto const params = sweptElevation (pattern.getElevationParams (), pattern);
  auto const inBand = heightMap.mapTo2D (
      unturnedInSpace (direction, spaceTurnOf (pattern)), params);
  return unshapedPosition (inBand, shapingOf (pattern));
}

float
ticksIntoFirstPass (index_t tick, index_t numTicks, PlayDirection direction,
                    float passTicks)
{
  if (numTicks == 0)
    return 0.f;

  auto const span = direction == PlayDirection::Bounce && numTicks > 1
                        ? numTicks - 1
                        : numTicks;
  auto const position
      = static_cast<float> (tick) / static_cast<float> (span);

  auto const travelled
      = direction == PlayDirection::Reverse ? 1.f - position : position;
  return travelled * passTicks;
}

void
setPassPhases (Pattern &pattern, float ticks, float ticksPerBar)
{
  auto const phase = [&] (Knob sweep) {
    return lfoPhaseAfter (pattern.getKnobStep (sweep), ticks, ticksPerBar);
  };

  pattern.setSpinPhase (phase (Knob::Spin));
  pattern.setReachLfoPhase (phase (Knob::Swell));
  pattern.setElevationLfoPhase (phase (Knob::Sway));
  pattern.setSqueezeXLfoPhase (phase (Knob::StretchX));
  pattern.setSqueezeYLfoPhase (phase (Knob::StretchY));
  pattern.setTiltLfoPhase (phase (Knob::TiltSweep));
  pattern.setRollLfoPhase (phase (Knob::RollSweep));
}

}
