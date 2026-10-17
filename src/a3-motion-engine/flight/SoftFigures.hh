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

#include <optional>

namespace a3
{

/** Floor units a beat round the big path at its own lap: the cruise. */
float cruisePace (int beatsPerBar, FlightTuning const &flight);

/** The mass of the body with `id`; a group's when there is none. */
float bodyMass (FlightBodies const &bodies, int id, FlightTuning const &flight);

/** A graze's straight pass: in at `a`, out at `b`. */
struct GrazeRun
{
  Vec2 a;
  Vec2 b;
};

/** The pass past a group at `group` of `mass`: the line square to the
 *  group's direction from the middle at the escort radius from it -- on the
 *  middle's side when that keeps grazeInnerRadius off the middle, else
 *  beyond the group -- from rim to rim of grazeOuterRadius. Nothing when no
 *  such line both stays off the middle and sweeps grazeMinSweepDegrees seen
 *  from it: a group at the middle, or too heavy near the wall. */
std::optional<GrazeRun> grazeRun (Vec2 group, float mass, FlightTuning const &flight,
                                  GameTuning const &tuning);

/** The circle an encircle flies round a group at `group` of `mass`: the
 *  escort's, widened until it subtends encircleMinDegrees from the middle. */
float encircleRadius (Vec2 group, float mass, FlightTuning const &flight,
                      GameTuning const &tuning);

/** A graze: round the middle to the pass's nearer end at cruise pace (shorter
 *  when the whole would pass grazeMaxBars), then straight along it at cruise
 *  pace. Over as it opens when there is nothing to pass. One ship. */
void planGraze (GamePlan &plan, FlightBodies const &bodies, int beatsPerBar,
                FlightTuning const &flight, GameTuning const &tuning);
OrbitPoint grazeGoal (GamePlan const &plan, int ship, double beats);

/** An encircle: encircleBars round the group at the escort's lap, from the
 *  leader's side of it, the way the leader flies; a partner opposite. */
void planEncircle (GamePlan &plan, std::array<ShipState, flightShips> const &ships,
                   FlightBodies const &bodies, int beatsPerBar, FlightTuning const &flight,
                   GameTuning const &tuning);
OrbitPoint encircleGoal (GamePlan const &plan, int ship, double beats);

}
