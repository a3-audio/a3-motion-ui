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

#include <array>

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

/** What a channel's pads are and which clip each belongs to.
 *
 *  Apart from the hardware that reads them, so that anything drawing pads —
 *  the controller page on the touchscreen — can be laid out from the same
 *  tables rather than from a second copy of them. A screen that decides for
 *  itself what its pads mean drifts away from the panel silently, and the
 *  first sign of it is a finger firing the wrong clip.
 */

/** Per channel. Two clip slots of four functions each. */
constexpr index_t numPadsPerChannel = 8;

enum class PadFunction
{
  PlayPause,
  Stop,
  Action,
  Settings,
};

/** Fixed per-channel pad-index -> function mapping (same for all channels). */
constexpr std::array<PadFunction, numPadsPerChannel> padFunctionByPadIndex{
  PadFunction::PlayPause, PadFunction::Action,
  PadFunction::PlayPause, PadFunction::Action,
  PadFunction::Stop,      PadFunction::Settings,
  PadFunction::Stop,      PadFunction::Settings,
};

/** Fixed per-channel pad-index -> clip-slot mapping (same for all channels).
 *  Slot 0 = upper quadrant {0,1,4,5}, slot 1 = lower quadrant {2,3,6,7}. */
constexpr std::array<index_t, numPadsPerChannel> slotForPadIndex{
  0, 0, 1, 1, 0, 0, 1, 1,
};

/** How many clips a channel's pads reach. */
constexpr index_t numPadSlots = 2;

}
