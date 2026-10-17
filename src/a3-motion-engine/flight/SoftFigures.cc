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

#include "SoftFigures.hh"

#include <a3-motion-engine/flight/FigureMath.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** Radians from `from` to `to` round the middle, the short way. */
float
turnTo (Vec2 from, Vec2 to)
{
  return std::remainder (angleOf (to) - angleOf (from), 2.f * pi<float> ());
}
}

float
cruisePace (int beatsPerBar, FlightTuning const &flight)
{
  if (beatsPerBar <= 0 || flight.orbitLapBars <= 0.f)
    return 0.f;
  return 2.f * pi<float> () * flight.orbitRadius
         / (flight.orbitLapBars * static_cast<float> (beatsPerBar));
}

float
bodyMass (FlightBodies const &bodies, int id, FlightTuning const &flight)
{
  for (auto i = 0; i < bodies.count && i < maxFlightBodies; ++i)
    if (bodies.body[static_cast<size_t> (i)].id == id)
      return bodies.body[static_cast<size_t> (i)].mass;
  return flight.groupMass;
}

std::optional<GrazeRun>
grazeRun (Vec2 group, float mass, FlightTuning const &flight, GameTuning const &tuning)
{
  auto const distance = group.getDistanceFromOrigin ();
  Vec2 const out = distance > 1e-6f ? group / distance : Vec2{ 1.f, 0.f };
  Vec2 const along{ -out.y, out.x };
  auto const escort = escortRadius (mass, flight);
  auto const rim = tuning.grazeOuterRadius;
  auto const halfSweep = 0.5f * radians (tuning.grazeMinSweepDegrees);
  for (auto const offset : { distance - escort, distance + escort })
    {
      if (offset < tuning.grazeInnerRadius || offset >= rim)
        continue;
      auto const half = std::sqrt (rim * rim - offset * offset);
      if (std::atan2 (half, offset) < halfSweep)
        continue;
      return GrazeRun{ out * offset + along * half, out * offset - along * half };
    }
  return std::nullopt;
}

float
encircleRadius (Vec2 group, float mass, FlightTuning const &flight, GameTuning const &tuning)
{
  auto const half = 0.5f * radians (tuning.encircleMinDegrees);
  return std::max (escortRadius (mass, flight), group.getDistanceFromOrigin () * std::sin (half));
}

void
planGraze (GamePlan &plan, FlightBodies const &bodies, int beatsPerBar,
           FlightTuning const &flight, GameTuning const &tuning)
{
  plan.climaxBeats = plan.passFromBeats = plan.endBeats = plan.startBeats;
  auto const run
      = grazeRun (plan.target, bodyMass (bodies, plan.targetBodyId, flight), flight, tuning);
  auto const pace = cruisePace (beatsPerBar, flight) * tuning.grazePaceFactor;
  if (!run || pace <= 0.f)
    return; // nothing to pass: over as it opens

  auto const from = plan.from[shipIndex (plan.leader)];
  std::array<Vec2, 2> ends{ run->a, run->b };
  if (std::abs (turnTo (from, ends[1])) < std::abs (turnTo (from, ends[0])))
    std::swap (ends[0], ends[1]);
  plan.run = ends;
  auto const turn = turnTo (from, ends[0]);
  plan.turn[shipIndex (plan.leader)] = turn < 0.f ? -1.f : 1.f;

  auto const fromRadius = from.getDistanceFromOrigin ();
  auto const inRadius = ends[0].getDistanceFromOrigin ();
  auto const approachLength
      = std::abs (turn) * 0.5f * (fromRadius + inRadius) + std::abs (inRadius - fromRadius);
  auto const pass = static_cast<double> (ends[0].getDistanceFrom (ends[1]) / pace);
  auto const most = std::max (0., static_cast<double> (tuning.grazeMaxBars) * beatsPerBar - pass);
  auto const approach = std::min (static_cast<double> (approachLength / pace), most);
  plan.passFromBeats = plan.climaxBeats = plan.startBeats + approach;
  plan.endBeats = plan.passFromBeats + pass;
}

OrbitPoint
grazeGoal (GamePlan const &plan, int ship, double beats)
{
  if (beats < plan.passFromBeats)
    return sweepGoal (plan.from[shipIndex (ship)], plan.run[0], plan.turn[shipIndex (ship)],
                      beats, plan.startBeats, plan.passFromBeats);
  return glideGoal (plan.run[0], plan.run[1], beats, plan.passFromBeats, plan.endBeats);
}

void
planEncircle (GamePlan &plan, std::array<ShipState, flightShips> const &ships,
              FlightBodies const &bodies, int beatsPerBar, FlightTuning const &flight,
              GameTuning const &tuning)
{
  plan.radius = encircleRadius (plan.target, bodyMass (bodies, plan.targetBodyId, flight), flight,
                                tuning);
  plan.lapBeats = static_cast<double> (flight.captureLapBars) * beatsPerBar;
  plan.climaxBeats = plan.startBeats;
  plan.endBeats = plan.startBeats + static_cast<double> (tuning.encircleBars) * beatsPerBar;

  auto const &lead = ships[shipIndex (plan.leader)];
  auto const fromGroup = lead.p - plan.target;
  plan.axis = angleOf (fromGroup);
  auto const sense = flightSense (fromGroup, lead.v);
  for (auto s = 0; s < flightShips; ++s)
    {
      plan.turn[shipIndex (s)] = sense;
      plan.part[shipIndex (s)] = s == plan.leader ? 0 : 1;
    }
}

OrbitPoint
encircleGoal (GamePlan const &plan, int ship, double beats)
{
  auto const opposite = pi<float> () * static_cast<float> (plan.part[shipIndex (ship)]);
  return circleGoal (plan.target, plan.radius, plan.lapBeats, plan.axis + opposite,
                     plan.turn[shipIndex (ship)], beats, plan.startBeats);
}

}
