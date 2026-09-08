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

#include <optional>

namespace a3
{

/** Where two taps put one of the global grid's three per-channel values, and
 *  whether they may.
 *
 *  Both answers are pulled out of A3MotionUIComponent so a test can reach
 *  them without a panel on the wire -- which is the whole point of the
 *  second one, and the one condition that had no other way of being checked.
 */

/** The rest position of a grid row, or nothing for a row nobody has decided
 *  about. Not a default of 0.5 for the unknown case: a row that fell through
 *  to a plausible value would look decided without being it. */
std::optional<float> channelValueRestPosition (int gridRow);

/** Whether the screen may put a channel value back at all.
 *
 *  It may exactly while no panel is answering. These three are physical
 *  controls when one is -- the pot for 3d, the two encoders for freq and Q --
 *  and the pot is absolute: a value the screen moved away from where the pot
 *  is standing would be snatched back by the next hair of movement, so the
 *  reset would read as a control that does not work.
 *
 *  Answered at runtime rather than at build time. This device is built with
 *  the hardware interface compiled in and still spends whole sessions with no
 *  panel on the wire, so a compile-time answer would be wrong for exactly the
 *  case the reset exists for.
 */
bool channelValueResetIsAllowed (bool hardwareIsAvailable);

}
