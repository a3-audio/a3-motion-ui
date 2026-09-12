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
  // Twelve o'clock for 3d and freq. The knobs sweep 270 degrees, so the
  // middle of the travel is the middle of the range, and a hand reaching for
  // "put that back" reaches for one place rather than two it has to
  // remember.
  //
  // Q is the exception, and it has been decided twice. It rested shut; it was
  // moved to twelve o'clock so a double tap would leave the three knobs
  // looking alike; the maintainer moved it back on 2026-09-12. The value wins
  // over the gesture -- a filter that still resonates after being put back
  // has not been put back -- and the far end agrees: the Airwindows Isolator3
  // this reaches rests its own Q at zero.
  switch (gridRow)
    {
    case channelRowThreeD:
    case channelRowFreq:
      return 0.5f;
    case channelRowQ:
      return 0.f;
    }

  return {};
}

bool
channelValueResetIsAllowed (bool hardwareIsAvailable)
{
  return !hardwareIsAvailable;
}

}
