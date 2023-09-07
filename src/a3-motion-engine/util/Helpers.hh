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

#include <chrono>

#include <JuceHeader.h>

#include <a3-motion-engine/tempo/Measure.hh>
#include <a3-motion-engine/util/Timing.hh>

namespace a3
{

template <class ClockT>
void
print (Timings<ClockT> const &timings)
{
  for (auto const &t : timings.get ())
    {
      juce::Logger::writeToLog (
          juce::String (t.tag) + ": "
          + juce::String (
              std::chrono::duration_cast<std::chrono::nanoseconds> (t.duration)
                  .count ()));
    }
}

juce::String toString (Pos const &position);
juce::String toString (Measure const &measure);

}
