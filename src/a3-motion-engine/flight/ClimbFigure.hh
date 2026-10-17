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

#include <a3-motion-engine/flight/GameFigures.hh>

namespace a3
{

/** A climb's lap, in bars, with `barsToGo` to its 1: the fastest in the last
 *  climbDoubleEveryBars, twice that in the stretch before, and so on, never
 *  slower than climbStartLapBars nor faster than climbFastestLapBars. */
float climbLapBars (double barsToGo, GameTuning const &tuning);

/** The laps a climb whose 1 is at `climaxBeats` flies between beats `from`
 *  and `to` (both at or before its 1), each stretch at its own lap. */
double climbLaps (double from, double to, double climaxBeats, int beatsPerBar,
                  GameTuning const &tuning);

/** Where a climb opened at `opening` lands: a drop the build names at
 *  least a bar away -- and only a previewed one is chained into a Slap, the
 *  live mood's has passed its 1 when it is heard -- else the next
 *  fallbackPhraseBars line a bar away, chaining nothing. */
struct ClimbLanding
{
  double climaxBeats = 0.;
  bool chainsIntoSlap = false;
};
ClimbLanding climbLanding (MusicCue const &cue, double opening, int beatsPerBar,
                           GameTuning const &tuning);

/** Whether the music says its drop opens on bar `bar`: a drop that began
 *  there, or a build that still names it for there. */
bool dropOpensOn (MusicCue const &cue, long long bar);

/** Sets `plan` up as a climb: from the cruise's radius to climbTopRadius by
 *  its 1, turning the leader's way from where the leader is, a crew spread
 *  evenly round the circle; after an unchained 1 it holds at the top at
 *  climbHoldLapBars for climbHoldBars, then ends. */
void planClimb (GamePlan &plan, std::array<ShipState, flightShips> const &ships,
                MusicCue const &cue, int beatsPerBar, FlightTuning const &flight,
                GameTuning const &tuning);

OrbitPoint climbGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar,
                      GameTuning const &tuning);

}
