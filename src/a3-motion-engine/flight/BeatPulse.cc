/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include "BeatPulse.hh"

#include <a3-motion-engine/tempo/TempoClock.hh>

#include <algorithm>

namespace a3
{

namespace
{
/** 1 + depth * (1 - beatFraction)^2 at `now`, already consolidated. */
float
breathingPulse (Measure const &now, float depth)
{
  auto const ticksPerBeat = static_cast<float> (TempoClock::getTicksPerBeat ());
  // A negative tick (a clock corrected backwards) survives consolidate; held
  // to the beat so the pulse stays within [1, 1 + depth].
  auto const beatFraction = std::clamp (
      static_cast<float> (now.tick ()) / ticksPerBeat, 0.f, 1.f);
  auto const sinceOnset = 1.f - beatFraction;
  return 1.f + depth * sinceOnset * sinceOnset;
}

Measure
consolidated (Measure now, int beatsPerBar)
{
  if (beatsPerBar > 0)
    now.consolidate (beatsPerBar);
  return now;
}

bool
onTheOne (Measure const &now)
{
  return now.beat () == 0;
}
}

float
gravityPulse (Measure now, int beatsPerBar, FlightTuning const &tuning)
{
  now = consolidated (now, beatsPerBar);
  if (tuning.gravityOnlyOnTheOne)
    return onTheOne (now) ? 1.f : 0.f;
  return breathingPulse (now, onTheOne (now) ? tuning.pulseDownbeatDepth
                                             : tuning.pulseDepth);
}

float
drawnPulse (Measure now, int beatsPerBar, FlightTuning const &tuning)
{
  if (!tuning.gravityOnlyOnTheOne)
    return gravityPulse (now, beatsPerBar, tuning);
  now = consolidated (now, beatsPerBar);
  return onTheOne (now) ? breathingPulse (now, tuning.pulseDownbeatDepth) : 1.f;
}

}
