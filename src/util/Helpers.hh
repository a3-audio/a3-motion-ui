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

#include <JuceHeader.h>

#include "../engine/TempoClock.hh"
#include "Timing.hh"

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

void
print (a3::TempoClock::Measure const &measure, juce::String prefix = "")
{
  juce::Logger::writeToLog (prefix + " " + juce::String (measure.bar) + "."
                            + juce::String (measure.beat) + "."
                            + juce::String (measure.tick) + ":"
                            + juce::String (measure.time_ns));
}

}
