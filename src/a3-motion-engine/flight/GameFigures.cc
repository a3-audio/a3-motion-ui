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

#include "GameFigures.hh"

#include <a3-motion-engine/util/Geometry.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
size_t
at (int ship)
{
  return static_cast<size_t> (ship);
}

bool
isShip (int ship)
{
  return ship >= 0 && ship < flightShips;
}

float
radians (float degrees)
{
  return degrees * pi<float> () / 180.f;
}

Vec2
polar (float angle, float radius)
{
  return { radius * std::cos (angle), radius * std::sin (angle) };
}

float
angleOf (Vec2 p)
{
  return std::atan2 (p.y, p.x);
}

Vec2
unitOr (Vec2 v, Vec2 fallback)
{
  auto const length = v.getDistanceFromOrigin ();
  return length > 1e-6f ? v / length : fallback;
}

OrbitPoint
standAt (Vec2 p)
{
  return { p, {} };
}

/** A goal sliding from `a` to `b` between beats `from` and `to`, with its
 *  velocity, so the ship follows it at the goal's pace. */
OrbitPoint
glide (Vec2 a, Vec2 b, double beats, double from, double to)
{
  if (to <= from || beats >= to)
    return standAt (b);
  auto const u = static_cast<float> (std::clamp ((beats - from) / (to - from), 0., 1.));
  return { a + (b - a) * u, (b - a) / static_cast<float> (to - from) };
}

/** Call & response: how many calls `part` has made before phrase `phrase`. */
int
callsBefore (int part, int phrase)
{
  return phrase < part ? 0 : (phrase - part + 1) / 2;
}

/** The first `count` of `order` sorted by `key`, equal keys in their given
 *  order. By insertion, not std::stable_sort: that one takes a temporary
 *  buffer from the heap, and a game is planned on the clock thread. */
template <typename Key>
void
sortStable (std::array<int, flightShips> &order, int count, Key const &key)
{
  for (auto i = 1; i < count; ++i)
    for (auto j = i; j > 0 && key (order[at (j)]) < key (order[at (j - 1)]); --j)
      std::swap (order[at (j)], order[at (j - 1)]);
}

/** +1 when `from` lies counter-clockwise of `target` seen from the middle,
 *  -1 clockwise, 0 on the same line through the middle. */
float
sideOf (Vec2 from, Vec2 target)
{
  auto const cross = target.x * from.y - target.y * from.x;
  return cross > 1e-6f ? 1.f : (cross < -1e-6f ? -1.f : 0.f);
}

Vec2
rotated (Vec2 p, float angle)
{
  auto const c = std::cos (angle);
  auto const s = std::sin (angle);
  return { c * p.x - s * p.y, s * p.x + c * p.y };
}

/** Where a ship's approach ends before its lane is applied: short of the
 *  target, on the side it comes in from. */
Vec2
approachEnd (GamePlan const &plan, int ship, GameTuning const &tuning)
{
  return plan.target
         + unitOr (plan.from[at (ship)] - plan.target, { 1.f, 0.f }) * tuning.approachStandOff;
}

/** A fake-out ship's lane, in radians round the middle: 0 for a lone ship,
 *  the crew spread a lane apart about the target. */
float
laneAngle (GamePlan const &plan, int ship, GameTuning const &tuning)
{
  auto const offset = static_cast<float> (plan.part[at (ship)])
                      - 0.5f * static_cast<float> (plan.crewSize - 1);
  return offset * radians (tuning.crewLaneDegrees);
}

void
planFakeOut (GamePlan &plan, juce::Random &dice, GameTuning const &tuning)
{
  // The veer is measured from the target, but the approach ends off to the
  // side it comes in on, and near the middle that offset is wide: veering
  // the same way would leave only the difference to be heard. So the game
  // veers away from the side its leader comes in on; the dice decide only
  // when the leader comes in on the target's own line through the middle.
  auto const thrown = dice.nextBool () ? 1.f : -1.f;
  auto const side = sideOf (plan.from[at (plan.leader)], plan.target);
  auto const way = side != 0.f ? -side : thrown;

  // A crew veers together, as one pack, each ship in its own lane, in the
  // order its approach already ends round the target, so nobody crosses
  // another's lane. Two halves veering apart met head-on in the strike.
  std::array<int, flightShips> order{};
  auto count = 0;
  for (auto s = 0; s < flightShips; ++s)
    if (plan.crew[at (s)])
      order[at (count++)] = s;
  auto const toward = angleOf (plan.target);
  sortStable (order, count, [&] (int ship) {
    return std::remainder (angleOf (approachEnd (plan, ship, tuning)) - toward,
                           2.f * pi<float> ());
  });
  for (auto lane = 0; lane < count; ++lane)
    {
      plan.part[at (order[at (lane)])] = lane;
      plan.turn[at (order[at (lane)])] = way;
    }
}

void
planFormation (GamePlan &plan)
{
  plan.axis = plan.targetBodyId != noBodyId ? angleOf (plan.target)
                                            : angleOf (plan.from[at (plan.leader)]);
  Vec2 const along{ -std::sin (plan.axis), std::cos (plan.axis) };
  // Places along the line in the order the ships already stand along it, so
  // nobody crosses another to reach its place.
  std::array<int, flightShips> order{};
  auto count = 0;
  for (auto s = 0; s < flightShips; ++s)
    if (plan.crew[at (s)])
      order[at (count++)] = s;
  sortStable (order, count,
              [&] (int ship) { return plan.from[at (ship)].getDotProduct (along); });
  for (auto place = 0; place < count; ++place)
    plan.part[at (order[at (place)])] = place;
}

void
planHideAndSeek (GamePlan &plan, std::array<ShipState, flightShips> const &ships,
                 GameTuning const &tuning)
{
  for (auto s = 0; s < flightShips; ++s)
    {
      if (!plan.crew[at (s)])
        continue;
      auto const &ship = ships[at (s)];
      auto const sense = ship.p.x * ship.v.y - ship.p.y * ship.v.x < 0.f ? -1.f : 1.f;
      plan.turn[at (s)] = angleOf (ship.p) + sense * radians (tuning.hideSideDegrees);
    }
}

void
planCallAndResponse (GamePlan &plan)
{
  plan.axis = angleOf (plan.from[at (plan.leader)]);
  for (auto s = 0; s < flightShips; ++s)
    plan.part[at (s)] = s == plan.leader ? 0 : 1;
}

OrbitPoint
fakeOutGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar,
             GameTuning const &tuning)
{
  auto const veerFrom = plan.climaxBeats - beatsPerBar;
  auto const strikeFrom = plan.climaxBeats - tuning.strikeBeats;
  auto const lane = laneAngle (plan, ship, tuning);

  if (beats < veerFrom)
    return glide (plan.from[at (ship)], rotated (approachEnd (plan, ship, tuning), lane), beats,
                  plan.startBeats, veerFrom);
  if (beats < strikeFrom)
    {
      auto const radius
          = std::max (plan.target.getDistanceFromOrigin (), tuning.veerMinRadius);
      return standAt (polar (angleOf (plan.target) + lane
                                 + plan.turn[at (ship)] * radians (tuning.veerDegrees),
                             radius));
    }
  return standAt (rotated (plan.target, lane));
}

OrbitPoint
formationGoal (GamePlan const &plan, int ship, double beats, GameTuning const &tuning)
{
  auto const place = plan.part[at (ship)];
  if (beats >= plan.climaxBeats)
    return standAt (polar (plan.axis + burstAngle (plan.crewSize, place), tuning.burstRadius));

  Vec2 const along{ -std::sin (plan.axis), std::cos (plan.axis) };
  auto const offset = (static_cast<float> (place)
                       - 0.5f * static_cast<float> (plan.crewSize - 1))
                      * tuning.formationSpacing;
  return standAt (polar (plan.axis, tuning.formationDistance) + along * offset);
}

OrbitPoint
hideAndSeekGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar,
                 GameTuning const &tuning)
{
  auto const hide = polar (plan.turn[at (ship)], tuning.hideRadius);
  auto const crossFrom = plan.climaxBeats - tuning.crossBeats;
  if (beats >= crossFrom)
    return standAt (polar (plan.turn[at (ship)] + pi<float> (), tuning.hideRadius));
  auto const slipEnd = std::min (plan.startBeats + tuning.slipBars * beatsPerBar, crossFrom);
  return glide (plan.from[at (ship)], hide, beats, plan.startBeats, slipEnd);
}

OrbitPoint
callAndResponseGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar,
                     GameTuning const &tuning)
{
  auto const part = plan.part[at (ship)];
  auto const home = plan.axis + static_cast<float> (part) * pi<float> ();
  auto const half = 0.5f * radians (tuning.callArcDegrees);
  if (beats < plan.climaxBeats)
    return standAt (polar (home - half, tuning.callRadius));

  auto const phrases = (beats - plan.climaxBeats) / beatsPerBar;
  auto const phrase = static_cast<int> (std::floor (phrases));
  // Each call runs from one end of the arc to the other; the next one back.
  auto const startSide = callsBefore (part, phrase) % 2 == 0 ? -half : half;
  if (phrase % 2 != part % 2)
    return standAt (polar (home + startSide, tuning.callRadius));

  auto const sweep = -2.f * startSide;
  auto const u = static_cast<float> (phrases - phrase);
  auto const angle = home + startSide + sweep * u;
  Vec2 const tangent{ -std::sin (angle), std::cos (angle) };
  return { polar (angle, tuning.callRadius),
           tangent * (tuning.callRadius * sweep / static_cast<float> (beatsPerBar)) };
}
}

int
targetBody (PilotTarget target, Vec2 from, FlightBodies const &bodies, FlightTuning const &flight)
{
  auto best = noBodyId;
  auto bestDistance = 0.f;
  for (auto i = 0; i < bodies.count && i < maxFlightBodies; ++i)
    {
      auto const &body = bodies.body[static_cast<size_t> (i)];
      if (body.mass <= 0.f)
        continue;
      auto wanted = false;
      switch (target.kind)
        {
        case PilotTargetKind::Nearest:
          wanted = true;
          break;
        case PilotTargetKind::Group:
          wanted = body.id == target.groupId;
          break;
        case PilotTargetKind::Crowd:
          wanted = juce::exactlyEqual (body.mass, flight.crowdMass);
          break;
        case PilotTargetKind::Hotspot:
          wanted = juce::exactlyEqual (body.mass, flight.hotspotMass);
          break;
        }
      if (!wanted)
        continue;
      auto const distance = body.at.getDistanceFrom (from);
      if (best == noBodyId || distance < bestDistance)
        {
          best = body.id;
          bestDistance = distance;
        }
    }
  return best;
}

std::optional<Vec2>
bodyPlace (FlightBodies const &bodies, int id)
{
  if (id == noBodyId)
    return std::nullopt;
  for (auto i = 0; i < bodies.count && i < maxFlightBodies; ++i)
    if (bodies.body[static_cast<size_t> (i)].id == id)
      return bodies.body[static_cast<size_t> (i)].at;
  return std::nullopt;
}

float
burstAngle (int crewSize, int place)
{
  // From one end of the line to the other. Checked against the line's places
  // at formationDistance 0.5 and spacing 0.2: no ship ends nearer than 45 deg
  // to where it stood (GameFigures test EveryShipBurstsAHeardBendFromItsPlaceOnTheOne).
  static constexpr std::array<std::array<float, flightShips>, flightShips> degrees{ {
      { 180.f, 0.f, 0.f, 0.f },
      { -90.f, 90.f, 0.f, 0.f },
      { -120.f, 180.f, 120.f, 0.f },
      { -150.f, -60.f, 60.f, 150.f },
  } };
  auto const n = std::clamp (crewSize, 1, flightShips);
  auto const i = std::clamp (place, 0, n - 1);
  return radians (degrees[at (n - 1)][at (i)]);
}

GamePlan
planGame (PilotGame game, int leader, std::array<bool, flightShips> const &crew,
          std::array<ShipState, flightShips> const &ships, PilotTarget target,
          FlightBodies const &bodies, MusicCue const &cue, double beats, int beatsPerBar,
          juce::Random &dice, FlightTuning const &flight, GameTuning const &tuning)
{
  GamePlan plan;
  plan.game = game;
  plan.leader = isShip (leader) ? leader : 0;
  plan.crew = crew;
  plan.crewSize = static_cast<int> (std::count (crew.begin (), crew.end (), true));
  plan.startBeats = beats;
  plan.climaxBeats = climaxBeatsFor (game, cue, beats, beatsPerBar, tuning);
  plan.endBeats = plan.climaxBeats + afterClimaxBeats (game, beatsPerBar, tuning);
  for (auto s = 0; s < flightShips; ++s)
    plan.from[at (s)] = ships[at (s)].p;

  auto const lead = plan.from[at (plan.leader)];
  plan.targetBodyId = targetBody (target, lead, bodies, flight);
  if (auto const place = bodyPlace (bodies, plan.targetBodyId))
    plan.target = *place;
  else
    plan.target = unitOr (-lead, { -1.f, 0.f }) * tuning.lonelyTargetRadius;

  switch (game)
    {
    case PilotGame::FakeOut:
      planFakeOut (plan, dice, tuning);
      break;
    case PilotGame::Formation:
      planFormation (plan);
      break;
    case PilotGame::HideAndSeek:
      planHideAndSeek (plan, ships, tuning);
      break;
    case PilotGame::CallAndResponse:
      planCallAndResponse (plan);
      break;
    case PilotGame::None:
      break;
    }
  return plan;
}

OrbitPoint
figureGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar,
            GameTuning const &tuning)
{
  if (!isShip (ship))
    return standAt ({});
  if (!plan.crew[at (ship)] || beatsPerBar <= 0)
    return standAt (plan.from[at (ship)]);

  switch (plan.game)
    {
    case PilotGame::FakeOut:
      return fakeOutGoal (plan, ship, beats, beatsPerBar, tuning);
    case PilotGame::Formation:
      return formationGoal (plan, ship, beats, tuning);
    case PilotGame::HideAndSeek:
      return hideAndSeekGoal (plan, ship, beats, beatsPerBar, tuning);
    case PilotGame::CallAndResponse:
      return callAndResponseGoal (plan, ship, beats, beatsPerBar, tuning);
    case PilotGame::None:
      break;
    }
  return standAt (plan.from[at (ship)]);
}

}
