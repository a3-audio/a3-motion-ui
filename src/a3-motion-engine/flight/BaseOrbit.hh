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

#pragma once

#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-engine/flight/FlightVec.hh>

#include <vector>

namespace a3
{

/** A point on the big flight path and how it moves there. */
struct OrbitPoint
{
  Vec2 at;
  Vec2 velocity; // floor units per beat
};

/** Where ship `channel`'s rabbit is `beats` after the start. The phase runs
 *  one lap per orbitLapBars, channel n a quarter lap behind n-1; the ellipse
 *  (semi-axes orbitRadius * (1 +- orbitEccentricity), centred in the room)
 *  turns once per orbitPrecessionBars. Locked to the bar, so a ship chasing
 *  it stays in time however far gravity throws it off.
 *
 *  `phaseOffset` (laps) moves the rabbit along its lap: a ship launched
 *  mid-flight starts its rabbit at the phase nearest to it rather than at its
 *  channel's slot. `radiusScale` scales the ellipse about the room's middle
 *  (an action's ~swell, phase B); 1 is the path as tuned. */
/** The lap phase (0..1) channel `channel`'s rabbit has at `beats` without
 *  an offset: its slot, a quarter lap behind channel n-1. */
float rabbitSlotPhase (double beats, int channel, int beatsPerBar,
                       FlightTuning const &tuning);

OrbitPoint rabbitAt (double beats, int channel, int beatsPerBar,
                     FlightTuning const &tuning, float phaseOffset = 0.f,
                     float radiusScale = 1.f);

/** The lap phase (0..1) on the ellipse nearest `p` at time `beats`.
 *  No allocation: safe on the clock thread. */
float nearestOrbitPhase (Vec2 p, double beats, int beatsPerBar,
                         FlightTuning const &tuning);

/** `count` points round the ellipse as it stands at `beats`, the last one on
 *  the first so the polyline closes itself. Allocates: message thread only. */
std::vector<Vec2> orbitGuidePoints (double beats, int beatsPerBar, int count,
                                    FlightTuning const &tuning);

}
