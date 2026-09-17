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

namespace a3
{

/** Whether a take keeps the lap that was still being played when it ended.
 *
 *  Recording runs on round and round inside the take's own length, each lap
 *  writing over the one before. Stop in the middle of a lap and the take is
 *  part new and part old, with a jump where they meet -- traced on
 *  2026-09-17: a take of 1024 ticks written for 6766, its first 622 ticks
 *  from the last lap and the rest from the one before, 1.14 radii apart.
 *  That seam is what reads as a hole in the trajectory.
 *
 *  So an unfinished lap is dropped and the take is the last whole one. The
 *  exception is a take that never finished a lap at all: dropping that would
 *  leave nothing to play. */
inline bool
takeKeepsPartialLap (long long ticksRecorded, long long lapTicks)
{
  if (lapTicks <= 0)
    return true;

  return ticksRecorded < lapTicks;
}

}
