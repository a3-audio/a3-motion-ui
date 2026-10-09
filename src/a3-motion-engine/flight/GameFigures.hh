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

#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/FlightWorld.hh>
#include <a3-motion-engine/flight/GameMoments.hh>
#include <a3-motion-engine/flight/ShipDynamics.hh>

#include <JuceHeader.h>

#include <array>
#include <optional>

namespace a3
{

/** One game as it was set up when it started: who plays which part, when
 *  its moments fall, what it is played against. Fixed size; the figure is
 *  worked out from it at every tick (figureGoal), nothing is stored per tick
 *  but the target's place. */
struct GamePlan
{
  PilotGame game = PilotGame::None;
  int leader = -1;
  std::array<bool, flightShips> crew{};
  int crewSize = 0;
  double startBeats = 0.;
  /** The game's 1 (climaxBeatsFor), and when it is over. */
  double climaxBeats = 0.;
  double endBeats = 0.;
  /** The group it is played against, or noBodyId; `target` is where that
   *  group is (kept up to date while it is dragged), or the point aimed at
   *  without one. */
  int targetBodyId = noBodyId;
  Vec2 target;
  /** Radians round the middle: the direction the formation faces, the line
   *  call & response's two sides lie on. */
  float axis = 0.f;
  /** Where each ship was when the game started. */
  std::array<Vec2, flightShips> from{};
  /** Formation: the ship's place in the line, from one end; call &
   *  response: 0 for the leader, 1 for its partner. */
  std::array<int, flightShips> part{};
  /** Fake-out: +1 or -1, the veer's way round; hide & seek: the angle, in
   *  radians, it hides at. */
  std::array<float, flightShips> turn{};
};

/** The id of the body `target` names, as seen from `from`: \nearest the
 *  nearest group of positive mass, \groupN the body of that id, \crowd and
 *  \hotspot the nearest body of that weight. A dead zone is nobody to play
 *  to. noBodyId when nothing fits. */
int targetBody (PilotTarget target, Vec2 from, FlightBodies const &bodies,
                FlightTuning const &flight);

/** Where the body with `id` is; nothing when there is none (any more). */
std::optional<Vec2> bodyPlace (FlightBodies const &bodies, int id);

/** Where place `place` of a line of `crewSize` bursts to, in radians from
 *  the formation's axis: every ship ends at least 45 deg from its place,
 *  seen from the middle, with the line's spacing as tuned. */
float burstAngle (int crewSize, int place);

/** `game` set up for `crew` at `beats`: its 1 from the cue, the target, the
 *  parts. `dice` throws a crew's fake-out ways round; a lone ship veers
 *  away from the side its approach comes in on. Pure apart from the dice. */
GamePlan planGame (PilotGame game, int leader, std::array<bool, flightShips> const &crew,
                   std::array<ShipState, flightShips> const &ships, PilotTarget target,
                   FlightBodies const &bodies, MusicCue const &cue, double beats,
                   int beatsPerBar, juce::Random &dice, FlightTuning const &flight,
                   GameTuning const &tuning);

/** What ship `ship` of `plan` steers for at `beats`: a point, and how it
 *  moves when the figure slides it. A ship outside the crew is told to stay
 *  where it started. No allocation: safe on the clock thread. */
OrbitPoint figureGoal (GamePlan const &plan, int ship, double beats, int beatsPerBar,
                       GameTuning const &tuning);

}
