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

#include "PatternProgressBar.hh"

#include <cmath>

namespace a3
{

int
progressBarDivisions (float lengthBeats, int beatsPerBar)
{
  auto const beats = juce::jmax (1, static_cast<int> (std::lround (lengthBeats)));
  auto const perBar = juce::jmax (1, beatsPerBar);

  // Bars once there is more than one of them, beats below that. A pattern of a
  // bar or less has no bars to count, and one of sixteen has too many beats.
  if (beats > perBar)
    return juce::jmax (1, beats / perBar);

  return beats;
}

}
