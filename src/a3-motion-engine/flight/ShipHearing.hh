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

#include <a3-motion-engine/flight/FlightMotion.hh>
#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** Where a flying ship is heard: `mapped` -- its floor point
 *  through its clip's band -- swung in height by the action's sway and leant
 *  by its tilt and roll, eased in by `weight` (0..1, smoothstep) the short way
 *  round. Without motion, at weight 0 or for an invalid `mapped` it is
 *  `mapped` exactly. A direction: unit length once eased in; a 180 degree
 *  lean flips at mid-ease, and limitTurn smooths it. No allocation: safe on the clock thread. */
Pos heardShip (Pos const &mapped, FlightMotion const &motion, float weight,
               double beats, int beatsPerBar, FlightTuning const &tuning);

/** A heard direction at most `maxDegrees` from where it was. */
struct TurnLimited
{
  Pos heard;
  /** False while the limit held it back: the next tick must limit again,
   *  or the rest of the way would be a jump. */
  bool caughtUp = true;
};

/** From `from` towards `to` along the great circle, at most `maxDegrees`.
 *  `to` itself when it is that close or `from` is invalid; a step along a fixed
 *  great circle when the two are opposite, which have no one of their own. */
TurnLimited limitTurn (Pos const &from, Pos const &to, float maxDegrees);

}
