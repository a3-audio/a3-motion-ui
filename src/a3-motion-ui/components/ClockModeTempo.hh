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

/** The clock modes, as the clock key steps through them. */
constexpr int clockModeInternal = 0;

/** The INT tempo to keep while the clock is EXT or PIO, after a step from
 *  `fromMode` to `toMode`. `kept` is what is kept so far, `engineTempo` the
 *  engine's tempo at the moment of the step. */
float internalTempoKept (int fromMode, int toMode, float kept,
                         float engineTempo);

}
