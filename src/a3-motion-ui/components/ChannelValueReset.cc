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

#include "ChannelValueReset.hh"

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

namespace a3
{

std::optional<float>
channelValueRestPosition (int gridRow)
{
  // Twelve o'clock on all three. The knobs sweep 270 degrees, so the middle
  // of the travel is the middle of the range, and a hand reaching for "put
  // that back" reaches for one place rather than three it has to remember.
  //
  // Q used to come to rest shut rather than centred, on the reasoning that a
  // resonance at rest is no resonance. That is true of the value and wrong
  // about the gesture: a double tap is "undo what I turned", and it should
  // leave the three knobs looking like each other.
  switch (gridRow)
    {
    case channelRowThreeD:
    case channelRowFreq:
    case channelRowQ:
      return 0.5f;
    }

  return {};
}

bool
channelValueResetIsAllowed (bool hardwareIsAvailable)
{
  return !hardwareIsAvailable;
}

}
