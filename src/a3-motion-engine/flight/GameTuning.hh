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

namespace a3
{

/** Every constant of the pilots' games, named, with its start value. Counted
 *  in bars and beats of the running meter, lengths in floor units (the room's
 *  edge at radius 1), angles in degrees.
 *
 *  Tags as in FlightTuning: [research] a published finding, [sweep] chosen by
 *  running the figures through this flight model (PilotGamesFlight.cc),
 *  [guess] neither: listen on the rig. The pro-DJ owns these defaults. */
struct GameTuning
{
  // When a game may land. Each game needs a run-up before its 1.
  float fakeOutLeadBars = 2.f;   // an approach of at least a bar, then the veer bar [guess]
  float formationLeadBars = 1.f; // a bar to line up [guess]
  float hideLeadBars = 3.f;      // the slip, a wait, the run across [guess]
  float callGatherBars = 1.f;    // to the two sides before the first call [guess]
  float afterClimaxBars = 1.f;   // a game holds its last figure this long after its 1 [guess]
  // With no fitting change ahead, a game lands on the next 4-bar line: the
  // shortest phrase a DJ counts [guess].
  int fallbackPhraseBars = 4;
  int hintHorizonBars = 8;  // a build's moment opens this many bars before its drop [guess]
  int grooveEveryBars = 16; // in a groove a moment opens at the start of every 16 bars [guess]
  int grooveWindowBars = 4; // and stays open this long [guess]
  int restBars = 4;         // after a game of their own the pilots wait this long [guess]
  float quietEnergy = 0.02f; // below this the music is too quiet for a game [guess]

  // A bend the room hears: at least 30 deg seen from the middle. The minimum
  // audible movement angle grows from about 5 deg in front to more than
  // 30 deg at the sides (Grantham 1986, JASA 79(6) 1939). [research]
  float heardBendDegrees = 30.f;

  // Fake-out.
  float approachStandOff = 0.2f;   // the approach ends this far short of the target [guess]
  float veerDegrees = 60.f;        // twice the heard bend [guess]
  float veerMinRadius = 0.5f;      // nearer the middle every direction is close [guess]
  float strikeBeats = 2.f;         // the dive at the target starts this long before the 1 [sweep]
  float lonelyTargetRadius = 0.6f; // with no group on the floor it aims across the room [guess]
  // A crew flies the figure in lanes this far apart round the middle, so no
  // two ships share a point and each is heard at its own place. The lanes
  // are the crew's one rigid figure; at this width the least gap between two
  // ships along the whole figure stays above the ships' separation core
  // (0.08) with the steering's settling included [sweep].
  float crewLaneDegrees = 22.f;
  // Near the middle an angle is a short way: the lanes widen until
  // neighbours stand the ships' separation core plus this apart anywhere
  // along the figure. The margin covers the circling of a ship held at a
  // point, which the flight's least speed makes about 0.05 wide [sweep],
  float crewLaneMargin = 0.1f;
  // and a crew's figure runs at least this far out, so a group at the very
  // middle still leaves room for a lane each [sweep].
  float crewLaneMinRadius = 0.3f;
  // A crew glides to its stand-offs at this pace at least, floor units per
  // beat, waiting first if the approach is long: above the ships' least
  // speed (0.08), so the goal is followed, not circled [sweep].
  float crewApproachPace = 0.12f;

  // Formation & scatter.
  // The line's distance from the middle. At 0.5 a line facing a group near
  // the rim stands on it, and the group's well draws two ships together
  // while they line up [sweep].
  float formationDistance = 0.4f;
  float formationSpacing = 0.2f;  // between two places in it [guess]
  float lineUpSettleBeats = 1.f;  // the line stands this long before the 1 [guess]
  float burstRadius = 0.85f;      // where the burst ends, inside the soft wall at 0.9 [guess]
  // The burst fans out from a point this far behind the line's centre, each
  // ship along the ray through its own place: the ways out never cross, the
  // ends of the line go round the sides and the middle goes back [sweep].
  float burstFocusBehind = 0.2f;

  // Hide & seek.
  float hideRadius = 0.85f;
  float hideSideDegrees = 75.f; // how far round it slips, the way it flies [guess]
  float slipBars = 2.f;         // slowly: a goal that creeps, not a dash [guess]
  float crossBeats = 3.f;       // the run across the room at full speed [sweep]

  // Call & response. A call is a bar: well over the ~0.3 s the ear needs to
  // hear a movement at all (Chandler & Grantham 1992, JASA 91(3) 1624).
  float callArcDegrees = 45.f; // one call, a heard bend with room to spare [guess]
  float callRadius = 0.7f;     // the big path's radius
  int exchanges = 2;           // each ship calls this many times
};

}
