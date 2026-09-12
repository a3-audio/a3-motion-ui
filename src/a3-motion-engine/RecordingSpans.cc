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

#include "RecordingSpans.hh"

#include <algorithm>

namespace a3
{

std::vector<UnwrittenSpan>
unwrittenSpans (std::vector<bool> const &written)
{
  auto const numTicks = static_cast<index_t> (written.size ());
  if (numTicks == 0)
    return {};

  auto const firstWritten = std::find (written.begin (), written.end (), true);
  if (firstWritten == written.end ())
    return { { 0u, numTicks } };

  // Walking from a written tick is what makes the stretch across the loop
  // point come out as one: it can then only ever end the walk, never be split
  // by its start.
  auto const start
      = static_cast<index_t> (firstWritten - written.begin ());

  std::vector<UnwrittenSpan> spans;
  index_t runBegin = 0;
  index_t runLength = 0;

  for (index_t step = 0; step < numTicks; ++step)
    {
      auto const tick = (start + step) % numTicks;

      if (!written[tick])
        {
          if (runLength == 0)
            runBegin = tick;
          ++runLength;
        }
      else if (runLength > 0)
        {
          spans.push_back ({ runBegin, runLength });
          runLength = 0;
        }
    }

  if (runLength > 0)
    spans.push_back ({ runBegin, runLength });

  return spans;
}

}
