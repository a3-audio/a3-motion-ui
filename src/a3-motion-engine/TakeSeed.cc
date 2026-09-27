/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include "TakeSeed.hh"

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/Pattern.hh>

namespace a3
{

void
seedTake (Pattern &take, Pattern const &from)
{
  auto const source = from.getTicks ().positions;
  auto const ticks = take.getNumTicks ();

  // Each tick of the take from the one at the same share of the clip. Nearest
  // rather than interpolated: a gap in the clip is a gap in the take, and a
  // jump stays a jump.
  if (!source.empty ())
    for (index_t tick = 0; tick < ticks; ++tick)
      {
        auto const at = static_cast<std::size_t> (
            static_cast<unsigned long long> (tick) * source.size () / ticks);
        if (source[at].isValid ())
          take.setTick (tick, source[at]);
      }

  applyLanes (take, from.getLanes ());
}

}
