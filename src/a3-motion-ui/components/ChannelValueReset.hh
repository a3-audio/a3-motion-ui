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

#include <a3-motion-ui/components/MixerControls.hh>

namespace a3
{

/** Where two taps put one of a channel's 3D, FREQ and Q.
 *
 *  Pulled out of A3MotionUIComponent so a test can reach it. Two taps are a
 *  turn to this value, panel on the wire or not (2026-10-06): the reset goes
 *  through setChannelPotValue like a drag does.
 */

/** The rest position of a channel pot, or nothing for one nobody has decided
 *  about. Not a default of 0.5 for the unknown case: a pot that fell through
 *  to a plausible value would look decided without being it. */
std::optional<float> channelPotRestPosition (ChannelPot pot);

}
