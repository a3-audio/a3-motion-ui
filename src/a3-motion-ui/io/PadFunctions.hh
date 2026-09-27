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

#include <a3-motion-engine/ClipSettings.hh> // numActionButtons
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

/** What a pad does since one clip per channel (2026-09-27): Play/Pause,
 *  PAGE -- which steps through the clip's pages -- and six action buttons.
 *  Named Page rather than Menu in code, because FunctionKey::Menu is already
 *  the settings menu; the pad may still be called MENU on the panel. */
enum class PadFunction
{
  PlayPause,
  Page,
  Action,
};

/** As the panel stands, two columns of four: Play where slot 1's Play was,
 *  Page where Stop was, and the six actions in reading order below them --
 *  A1 A2 / A3 A4 / A5 A6. The firmware only knows pad indices, so nothing
 *  on the device changes; what a pad means changes here. */
constexpr std::array<PadFunction, numPadsPerChannel> padFunctionByPadIndex{
  PadFunction::PlayPause, PadFunction::Action,
  PadFunction::Action,    PadFunction::Action,
  PadFunction::Page,      PadFunction::Action,
  PadFunction::Action,    PadFunction::Action,
};

/** Which action button a pad is, or -1 for Play and Page. */
constexpr std::array<int, numPadsPerChannel> actionButtonForPad{
  -1, 0, 2, 4, -1, 1, 3, 5,
};

/** One clip per channel. Kept as a count until the slot dimension is taken
 *  out of the containers (step G of the one-clip plan). */
constexpr index_t numPadSlots = 1;

/** The pad of Play/Pause or Page. */
constexpr index_t
padIndexFor (PadFunction function)
{
  for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
    if (padFunctionByPadIndex[pad] == function)
      return pad;
  return 0;
}

/** The pad of action button `button` (0..5). */
constexpr index_t
padIndexForAction (int button)
{
  for (index_t pad = 0; pad < numPadsPerChannel; ++pad)
    if (actionButtonForPad[pad] == button)
      return pad;
  return 0;
}

}
