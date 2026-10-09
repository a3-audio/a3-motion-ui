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

#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-engine/flight/FlightVec.hh>

#include <vector>

namespace a3
{

/** Which line a channel's ship has on the ball in FPV.
 *
 *  A CLIP ship flies its clip, so its trajectory is where it goes. A ship
 *  flying ORBIT does not: the trajectory under it is a path it has left,
 *  and what it follows is the big orbit, or an escort's circle round its
 *  group. A ship in a game steers to a figure no line here knows, so it has
 *  none -- the game's own line and word mark it. */
enum class ShipPathShown
{
  Trajectory,
  Orbit,
  Escort,
  None,
};

/** What decides it, for one channel, this frame. */
struct ShipPathFacts
{
  bool fpv = false;
  /** The engine flies it ORBIT (MotionEngine::getFlightMode). */
  bool orbit = false;
  /** It plays in a game -- an ORBIT ship, or a CLIP ship borrowed into it,
   *  which flies as well (MotionEngine::gameOf). */
  bool inGame = false;
  /** It escorts a group (escortView). */
  bool escorting = false;
};

ShipPathShown shipPathShown (ShipPathFacts const &facts);

/** Whether the channel's own clip trajectory is drawn. */
bool drawsTrajectory (ShipPathShown shown);

/** How brightly the line maps draw a channel's line: a trajectory at full,
 *  an orbit or an escort's circle quieter -- the ship's path, not a figure
 *  someone chose. */
constexpr float orbitLineLevel = 0.35f;
float lineLevelOf (ShipPathShown shown);

/** How many points an escort's circle is drawn with. */
constexpr int escortPathPointCount = 48;

/** `count` points round an escort's circle on the ships' disc, the last on
 *  the first. Points past the rim stand on it, where the disc's wall holds
 *  the ship. Empty for a circle of no size or fewer than three points. */
std::vector<Vec2> escortPathPoints (Vec2 centre, float radius, int count);

/** The floor points of the line a ship that does not fly its clip shows:
 *  the orbit (`guide`, the shared ellipse as it stands now) or its escort's
 *  circle round body `escortId`. Empty for a trajectory, a game, or an
 *  escort whose group is gone. */
std::vector<Vec2> shipPathPoints (ShipPathShown shown,
                                  std::vector<Vec2> const &guide,
                                  FlightBodies const &bodies, int escortId,
                                  FlightTuning const &tuning);

}
