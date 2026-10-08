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

#include <cmath>

namespace a3
{

/** Whether the breath holds the ships at `beats`: for the last beat of every
 *  bar, so they restart on the one.
 *
 *  The MJ lab's cure for ships that just circle (2026-10-08, playbook rule
 *  18): a stop on the grid makes the restart an event for the whole floor.
 *  One whole beat, because a shorter stop is under the ~336 ms the ear
 *  integrates over; once a bar, because two stops a bar are a stutter. */
inline bool
breathHolds (double beats, int beatsPerBar)
{
  // Under three beats a bar the last beat is half the bar or all of it: a
  // stutter or a standstill, not a breath.
  constexpr int fewestBeatsToBreathe = 3;
  if (beatsPerBar < fewestBeatsToBreathe || beats < 0.)
    return false;
  auto const inBar = std::fmod (beats, static_cast<double> (beatsPerBar));
  return inBar >= static_cast<double> (beatsPerBar - 1);
}

}
