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
#include <limits>

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

/** A goal sweeping round the middle from `a` to `b` between beats `from`
 *  and `to`, radius and angle each at an even pace, going round the way
 *  `turning` says (+1 counter-clockwise, -1 clockwise) even when the other
 *  way is shorter, with its velocity. */
OrbitPoint
sweep (Vec2 a, Vec2 b, float turning, double beats, double from, double to)
{
  if (to <= from || beats >= to)
    return standAt (b);
  auto const u = static_cast<float> (std::clamp ((beats - from) / (to - from), 0., 1.));
  auto const span = static_cast<float> (to - from);
  auto const radiusA = a.getDistanceFromOrigin ();
  auto const radiusB = b.getDistanceFromOrigin ();
  auto turn = std::remainder (angleOf (b) - angleOf (a), 2.f * pi<float> ());
  if (turning * turn < 0.f)
    turn += turning * 2.f * pi<float> ();
  auto const angle = angleOf (a) + turn * u;
  auto const radius = radiusA + (radiusB - radiusA) * u;
  Vec2 const out{ std::cos (angle), std::sin (angle) };
  Vec2 const along{ -out.y, out.x };
  return { out * radius, (out * (radiusB - radiusA) + along * (radius * turn)) / span };
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

/** Where a ship's approach ends before its lane is applied: short of the
 *  target, on the side it comes in from. */
Vec2
approachEnd (GamePlan const &plan, int ship, GameTuning const &tuning)
{
  return plan.target
         + unitOr (plan.from[at (ship)] - plan.target, { 1.f, 0.f }) * tuning.approachStandOff;
}

/** How far from the origin the segment a-b comes. */
float
nearestToTheMiddle (Vec2 a, Vec2 b)
{
  auto const along = b - a;
  auto const length = along.getDistanceSquaredFromOrigin ();
  auto const u = length > 0.f ? std::clamp (-a.getDotProduct (along) / length, 0.f, 1.f) : 0.f;
  return (a + along * u).getDistanceFromOrigin ();
}

/** How far out the veer runs: the target's radius, but never nearer the
 *  middle than veerMinRadius. */
float
veerRadius (GamePlan const &plan, GameTuning const &tuning)
{
  return std::max (plan.target.getDistanceFromOrigin (), tuning.veerMinRadius);
}

/** The circle a crew's figure runs on: the target's radius, but never
 *  nearer the middle than crewLaneMinRadius. */
float
laneRadius (GamePlan const &plan, GameTuning const &tuning)
{
  return std::max (plan.target.getDistanceFromOrigin (), tuning.crewLaneMinRadius);
}

/** The point a ship veers to, `lane` radians round the middle from the
 *  figure's own: the target's direction turned the ship's way round. */
Vec2
veerPoint (GamePlan const &plan, int ship, float lane, GameTuning const &tuning)
{
  return polar (angleOf (plan.target) + lane + plan.turn[at (ship)] * radians (tuning.veerDegrees),
                veerRadius (plan, tuning));
}

/** A crew's stand-off, `lane` round from the figure's own: on the lane
 *  circle, approachStandOff round from the target on the side the crew
 *  comes in on, which is the side it veers away from. */
Vec2
crewStandOff (GamePlan const &plan, int ship, float lane, GameTuning const &tuning)
{
  auto const radius = laneRadius (plan, tuning);
  auto const chord = std::min (1.f, tuning.approachStandOff / (2.f * radius));
  auto const round = 2.f * std::asin (chord);
  return polar (angleOf (plan.target) - plan.turn[at (ship)] * round + lane, radius);
}

/** Where a crew ship strikes, `lane` round from the target: its lane's
 *  point beside it on the lane circle. */
Vec2
crewStrike (GamePlan const &plan, float lane, GameTuning const &tuning)
{
  return polar (angleOf (plan.target) + lane, laneRadius (plan, tuning));
}

/** Radians between neighbouring lanes: crewLaneDegrees, or wider where the
 *  figure runs near the middle. Every lane flies the same figure turned
 *  round the middle, in step, so two neighbours stand 2 r sin (step / 2)
 *  apart where the figure passes radius r: the step is widened until that
 *  is laneClearance at the nearest the figure comes to the middle. */
float
laneStep (GamePlan const &plan, GameTuning const &tuning)
{
  auto const leader = plan.leader;
  auto const standOff = crewStandOff (plan, leader, 0.f, tuning);
  auto const veer = veerPoint (plan, leader, 0.f, tuning);
  auto const strike = crewStrike (plan, 0.f, tuning);
  // The veer sweeps round the middle, never nearer than the lane circle or
  // the veer's; the strike dives along a chord.
  auto const nearest = std::max (1e-3f, std::min ({ standOff.getDistanceFromOrigin (),
                                                    veer.getDistanceFromOrigin (),
                                                    nearestToTheMiddle (veer, strike) }));
  auto const chord = std::min (1.f, plan.laneClearance / (2.f * nearest));
  return std::max (radians (tuning.crewLaneDegrees), 2.f * std::asin (chord));
}

/** Lane `lane` of the crew, in radians round the middle: 0 for a lone ship,
 *  the crew spread a lane apart about the figure. */
float
laneAngleOf (GamePlan const &plan, int lane, GameTuning const &tuning)
{
  if (plan.crewSize <= 1)
    return 0.f;
  auto const offset = static_cast<float> (lane) - 0.5f * static_cast<float> (plan.crewSize - 1);
  return offset * laneStep (plan, tuning);
}

float
laneAngle (GamePlan const &plan, int ship, GameTuning const &tuning)
{
  return laneAngleOf (plan, plan.part[at (ship)], tuning);
}

/** Where the approach ends: a lone ship short of the target on the side it
 *  comes in from, a crew ship at its lane's stand-off. */
Vec2
approachGoal (GamePlan const &plan, int ship, GameTuning const &tuning)
{
  if (plan.crewSize <= 1)
    return approachEnd (plan, ship, tuning);
  return crewStandOff (plan, ship, laneAngle (plan, ship, tuning), tuning);
}

/** Where a fake-out ship strikes: a lone ship the target itself, a crew
 *  ship its lane's point beside it. */
Vec2
strikePoint (GamePlan const &plan, int ship, GameTuning const &tuning)
{
  if (plan.crewSize <= 1)
    return plan.target;
  return crewStrike (plan, laneAngle (plan, ship, tuning), tuning);
}

void
planFakeOut (GamePlan &plan, juce::Random &dice, FlightTuning const &flight,
             GameTuning const &tuning)
{
  plan.laneClearance = flight.separationSoftening + tuning.crewLaneMargin;
  // The veer is measured from the target, but the approach ends off to the
  // side it comes in on, and near the middle that offset is wide: veering
  // the same way would leave only the difference to be heard. So the game
  // veers away from the side its leader comes in on; the dice decide only
  // when the leader comes in on the target's own line through the middle.
  auto const thrown = dice.nextBool () ? 1.f : -1.f;
  auto const side = sideOf (plan.from[at (plan.leader)], plan.target);
  auto const way = side != 0.f ? -side : thrown;

  // A crew veers together, as one pack, each ship in its own lane: the
  // same figure turned a lane further round the middle, flown in step, so
  // the crew keeps its order through every phase and no two ships cross.
  // The lanes are dealt so that the crew's glides to its stand-offs are the
  // shortest in all: two glides that crossed could be swapped for two
  // shorter ones, so the shortest never cross. Two halves veering apart met
  // head-on in the strike.
  std::array<int, flightShips> crew{};
  auto count = 0;
  for (auto s = 0; s < flightShips; ++s)
    if (plan.crew[at (s)])
      {
        crew[at (count++)] = s;
        plan.turn[at (s)] = way;
      }
  std::array<int, flightShips> lanes{};
  for (auto lane = 0; lane < count; ++lane)
    lanes[at (lane)] = lane;
  auto dealt = lanes;
  auto shortest = std::numeric_limits<float>::infinity ();
  do
    {
      auto length = 0.f;
      for (auto i = 0; i < count; ++i)
        {
          auto const ship = crew[at (i)];
          auto const standOff
              = crewStandOff (plan, ship, laneAngleOf (plan, lanes[at (i)], tuning), tuning);
          length += plan.from[at (ship)].getDistanceFrom (standOff);
        }
      if (length < shortest)
        {
          shortest = length;
          dealt = lanes;
        }
    }
  while (std::next_permutation (lanes.begin (), lanes.begin () + count));
  for (auto i = 0; i < count; ++i)
    plan.part[at (crew[at (i)])] = dealt[at (i)];
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
    {
      auto const from = plan.from[at (ship)];
      auto const standOff = approachGoal (plan, ship, tuning);
      if (plan.crewSize <= 1)
        return glide (from, standOff, beats, plan.startBeats, veerFrom);
      // A crew's glide keeps a pace a ship can fly: a goal creeping below
      // the ships' least speed is circled, not followed, and the crew would
      // reach its stand-offs out of step. So the glide waits, then goes.
      auto const beatsAtPace = from.getDistanceFrom (standOff) / tuning.crewApproachPace;
      return glide (from, standOff, beats, std::max (plan.startBeats, veerFrom - beatsAtPace),
                    veerFrom);
    }
  // A lone ship's goal jumps, and the ship flies hard for it. A crew's goals
  // sweep, so the pack flies in step and its lanes stay a lane apart: the
  // veer round the middle over the veer bar, the strike a dive from the
  // veer point.
  if (beats < strikeFrom)
    {
      if (plan.crewSize <= 1)
        return standAt (veerPoint (plan, ship, lane, tuning));
      return sweep (approachGoal (plan, ship, tuning), veerPoint (plan, ship, lane, tuning),
                    plan.turn[at (ship)], beats, veerFrom, strikeFrom);
    }
  if (plan.crewSize <= 1)
    return standAt (strikePoint (plan, ship, tuning));
  return glide (veerPoint (plan, ship, lane, tuning), strikePoint (plan, ship, tuning), beats,
                strikeFrom, plan.climaxBeats);
}

OrbitPoint
formationGoal (GamePlan const &plan, int ship, double beats, GameTuning const &tuning)
{
  auto const place = plan.part[at (ship)];
  if (beats >= plan.climaxBeats)
    return standAt (
        polar (plan.axis + burstAngle (plan.crewSize, place, tuning), tuning.burstRadius));

  Vec2 const along{ -std::sin (plan.axis), std::cos (plan.axis) };
  auto const offset = (static_cast<float> (place)
                       - 0.5f * static_cast<float> (plan.crewSize - 1))
                      * tuning.formationSpacing;
  auto const placeAt = polar (plan.axis, tuning.formationDistance) + along * offset;
  // Glided to, not rushed: a ship sent straight at a place 0.2 from its
  // neighbour's overshoots it and the two swing through each other for bars.
  auto const standFrom = std::max (plan.startBeats, plan.climaxBeats - tuning.lineUpSettleBeats);
  return glide (plan.from[at (ship)], placeAt, beats, plan.startBeats, standFrom);
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
burstAngle (int crewSize, int place, GameTuning const &tuning)
{
  // Each ship bursts along the ray from one focus behind the line's centre
  // through its own place, out to the burst radius: rays from one point
  // never cross, and the further out along the line a ship stands, the
  // further round it goes. In the axis frame: x along the axis, y along the
  // line.
  auto const n = std::clamp (crewSize, 1, flightShips);
  auto const i = std::clamp (place, 0, n - 1);
  auto const offset = (static_cast<float> (i) - 0.5f * static_cast<float> (n - 1))
                      * tuning.formationSpacing;
  Vec2 const focus{ tuning.formationDistance + tuning.burstFocusBehind, 0.f };
  Vec2 const ray{ -tuning.burstFocusBehind, offset };
  // |focus + u ray| = burstRadius, the root beyond the place (u > 1 when the
  // place lies inside the burst circle, as it does at the tuned distances).
  auto const a = ray.getDistanceSquaredFromOrigin ();
  auto const b = 2.f * focus.getDotProduct (ray);
  auto const c = focus.getDistanceSquaredFromOrigin () - tuning.burstRadius * tuning.burstRadius;
  auto const root = std::sqrt (std::max (0.f, b * b - 4.f * a * c));
  auto const u = a > 0.f ? (-b + root) / (2.f * a) : 0.f;
  auto const out = focus + ray * u;
  return std::atan2 (out.y, out.x);
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

  // Hide & seek and call & response are played against no group: whatever
  // the order names, they follow none and are spared none.
  auto const lead = plan.from[at (plan.leader)];
  auto const playedAgainstAGroup = game == PilotGame::FakeOut || game == PilotGame::Formation;
  plan.targetBodyId = playedAgainstAGroup ? targetBody (target, lead, bodies, flight) : noBodyId;
  if (auto const place = bodyPlace (bodies, plan.targetBodyId))
    plan.target = *place;
  else
    plan.target = unitOr (-lead, { -1.f, 0.f }) * tuning.lonelyTargetRadius;

  switch (game)
    {
    case PilotGame::FakeOut:
      planFakeOut (plan, dice, flight, tuning);
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
