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

#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/flight/FlightTuning.hh>

#include <optional>

namespace a3
{

/** What an action's Motion keys mean to a ship in ORBIT (FPV phase B).
 *
 *  Only the keys the script assigned are set: a clip's own spin or sway never
 *  steers the orbit, so a ship with no action flies exactly as before. Steps
 *  are TempoLfo steps (bars per cycle, sign a direction), leans quarter turns,
 *  `speedLog2` the clip's speed exponent (higher is slower).
 *
 *  spin, speedLog2, swell move the ship on the floor (FlightWorld); sway and
 *  the leans move where it is heard (ShipHearing). The squeezes, rotate and
 *  the elevation keys have no flight meaning and are not here. */
struct FlightMotion
{
  std::optional<int> spin;      // ~spin: bars per lap, sign the clip's sense
  std::optional<int> sway;      // ~sway: the height swing's cycle
  std::optional<int> swell;     // ~swell: the path breathing out (+) or in (-)
  std::optional<float> tilt;    // ~tilt
  std::optional<float> roll;    // ~roll
  std::optional<int> tiltSweep; // ~tswp
  std::optional<int> rollSweep; // ~rswp
  std::optional<int> speedLog2; // ~speedLog2: the lap times 2^-speedLog2

  bool any () const;
  /** Whether it moves the ship on the floor, not only how it is heard. */
  bool movesTheFloor () const;
};

/** Laps per beat of the base path, in the floor's counter-clockwise sense
 *  (the rabbit's); 0 when it stands (orbitLapBars <= 0). */
float baseLapsPerBeat (int beatsPerBar, FlightTuning const &tuning);

/** Laps per beat a patrolling ship is to fly under `motion`: the spin's bars
 *  per lap in the clip's sense (or the base path's lap without a spin; 0 for
 *  a spin of 0), times 2^-speedLog2, never more than
 *  angularCapDegreesPerBeat / 360 either way. */
float wantedLapsPerBeat (FlightMotion const &motion, int beatsPerBar,
                         FlightTuning const &tuning);

/** Beats per cycle of TempoLfo `step`, never fewer than `shortestBeats`;
 *  0 for step 0, which has no cycle. */
float cycleBeats (int step, int beatsPerBar, float shortestBeats);

/** The shortest cycle in which lfoTravel's raised cosine over `degrees`
 *  stays under `capDegreesPerBeat`: its steepest point is pi * degrees per
 *  cycle. */
float shortestTravelBeats (float degrees, float capDegreesPerBeat);

/** How much the base ellipse is scaled at `beats` by the swell: 1 at the
 *  cycle's start and end, the sign's end at its middle. 1 without a swell. */
float swellScale (FlightMotion const &motion, double beats, int beatsPerBar,
                  FlightTuning const &tuning);

/** Degrees the heard ship is lifted (+) or lowered (-) at `beats`: out and
 *  back over swayTravelDegrees, positive down first (a clip's sway takes its
 *  base towards the south pole). 0 without a sway. */
float swayDegrees (FlightMotion const &motion, double beats, int beatsPerBar,
                   FlightTuning const &tuning);

/** The heard plane's lean at `beats`: tilt and roll, each turned round by its
 *  sweep as a clip's lean is, one whole turn in no fewer than
 *  360 / angularCapDegreesPerBeat beats. */
SpaceTurn flightLean (FlightMotion const &motion, double beats,
                      int beatsPerBar, FlightTuning const &tuning);

}
