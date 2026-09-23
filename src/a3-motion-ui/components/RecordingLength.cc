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

#include "RecordingLength.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

int
recordingLengthBeats (int clipLengthBeats, int storedRecordLengthLog2,
                      int beatsPerBar)
{
  if (clipLengthBeats > 0)
    return clipLengthBeats;

  auto const bars = std::exp2 (static_cast<double> (storedRecordLengthLog2));
  auto const beats = bars * std::max (1, beatsPerBar);

  return std::max (1, static_cast<int> (std::lround (beats)));
}

}
