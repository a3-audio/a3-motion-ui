/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

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

#include <a3-motion-engine/Pattern.hh>

#include <optional>
#include <vector>

namespace a3
{

/** How long a take's closing move lasts, counted in sixteenths of a beat.
 *
 *  Zero is a hard join: the take holds what was last played and jumps. Above
 *  zero it travels back instead, over that much time, and the time is taken
 *  out of the end of the take -- the closing move is written over the stale
 *  pass that sits there, not added to the loop.
 *
 *  A length, not a mode, because that is what it always was: the code used to
 *  work one out from the trajectory's own speed and cap it with two guessed
 *  limits. Both guesses belonged to whoever is listening. */
constexpr index_t ticksPerFadeStep (index_t ticksPerBeat)
{
  return ticksPerBeat / 16;
}

/** Fill every stretch `pattern` never wrote.
 *
 *  A stretch in the middle is held at the position before it and then jumps --
 *  always, whatever `fadeTicks` says, because a finger lifted there lifted on
 *  purpose. Only the take's own join follows it.
 *
 *  `stopTick` is where the take stopped: the last tick the freshest pass
 *  wrote. Recording in Loop runs several passes, so the ticks after it still
 *  carry an earlier one, and the edge between them is where the motion visibly
 *  breaks. Nothing is missing there, so filling holes never reached it. Left
 *  out, the loop point is used instead.
 *
 *  Does nothing to a pattern that wrote nothing at all -- the caller discards
 *  such a take rather than filling it with guesses. */
void closeRecordingSeams (Pattern &pattern, index_t fadeTicks,
                          std::optional<index_t> stopTick = {});

/** Write the take's closing move again at a new length, from the two played
 *  positions at either end of the join rather than from the last fill -- so
 *  changing it repeatedly cannot walk the join somewhere else.
 *
 *  A playback setting, not a recording one: it can be changed long after the
 *  take was played, which is why the join's position travels with the pattern
 *  and survives a restart. */
void applyFade (Pattern &pattern, index_t fadeTicks);

/** How long the closing move needs to travel at the take's own speed.
 *
 *  A fixed setting is as likely to crawl as to race: how long the move needs
 *  depends on how far it has to go and how fast the take was moving, and both
 *  come out of the take. This is the length the fade starts at, and the value
 *  shown in the bar, so it can be turned from there. Zero when the take has no
 *  join worth closing or never moved. */
index_t naturalFadeTicks (std::vector<Pos> const &ticks, index_t join);

}
