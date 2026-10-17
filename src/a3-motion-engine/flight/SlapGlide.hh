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

#include <a3-motion-engine/flight/FigureStance.hh>
#include <a3-motion-engine/flight/FlightVec.hh>
#include <a3-motion-engine/flight/GameTuning.hh>
#include <a3-motion-engine/util/Types.hh>

#include <optional>

namespace a3
{

/** How long the ear takes to reach a new hold point, in beats: slapGlideBeats,
 *  and up-down at slapVerticalRateFactor of that rate. */
double slapGlideLength (GlidePath path, GameTuning const &tuning);

/** The heard direction `elapsed` beats into a glide from `from` to `to`:
 *  azimuth and elevation each at an even pace -- not a great circle, which
 *  would cut over the listeners' heads -- along `path` (see GlidePath). `to`
 *  once the glide is over, or when either end is invalid. No allocation. */
Pos slapGlideAt (Pos const &from, Pos const &to, GlidePath path, double elapsed,
                 GameTuning const &tuning);

/** One ship's slap glides, as the engine runs them tick by tick. Pure, fixed
 *  size, clock thread. */
class SlapGlider
{
public:
  /** This tick's heard direction while a glide runs, else nothing (the caller
   *  holds the turn to its limit as before). A glide starts when the stance
   *  holds the ship at a new point -- a hold that begins or moves -- from
   *  `lastHeard` towards `target`, the new point as heard; its last tick is
   *  `target` itself. Nothing starts from an invalid `lastHeard`. */
  std::optional<Pos> step (FigureStance const &stance, Vec2 holdAt, Pos const &lastHeard,
                           Pos const &target, double beats, GameTuning const &tuning);
  void reset ();

private:
  bool _held = false;
  Vec2 _heldAt;
  bool _gliding = false;
  Pos _from = Pos::invalid;
  GlidePath _path = GlidePath::Shortest;
  double _startBeats = 0.;
};

}
