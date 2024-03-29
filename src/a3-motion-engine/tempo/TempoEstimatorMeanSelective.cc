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

#include "TempoEstimatorMeanSelective.hh"

#include <algorithm>
#include <chrono>
#include <limits>
#include <numeric>

#include <JuceHeader.h>

namespace a3
{

TempoEstimatorMeanSelective::TempoEstimatorMeanSelective (
    int numSelectedDeltas)
    : _numSelectedDeltas (numSelectedDeltas)
{
}

void
TempoEstimatorMeanSelective::estimateTempo ()
{
  auto deltaTs = getTapTimeDeltas ();
  jassert (deltaTs.size () > 0);

  ClockT::duration sumDeltaT{ 0 };
  for (auto &deltaT : deltaTs)
    {
      sumDeltaT += deltaT;
    }

  // casting because chrono::abs below chokes on signedness with
  // gcc 13.1
  jassert (deltaTs.size () <= std::numeric_limits<ssize_t>::max ());
  auto deltaTAverage = sumDeltaT / static_cast<ssize_t> (deltaTs.size ());

  std::sort (deltaTs.begin (), deltaTs.end (),
             [deltaTAverage] (auto &a, auto &b) {
               auto const deltaA = std::chrono::abs (a - deltaTAverage);
               auto const deltaB = std::chrono::abs (b - deltaTAverage);
               return deltaA < deltaB;
             });

  juce::Logger::writeToLog ("-----------------");
  for (auto &deltaT : deltaTs)
    {
      juce::Logger::writeToLog (juce::String (
          std::abs (std::chrono::duration_cast<std::chrono::milliseconds> (
                        deltaT - deltaTAverage)
                        .count ())));
    }

  jassert (deltaTs.size () < std::numeric_limits<int>::max ());
  auto numSelected
      = std::min (_numSelectedDeltas, static_cast<int> (deltaTs.size ()));
  sumDeltaT = std::accumulate (
      deltaTs.begin (), deltaTs.begin () + numSelected, ClockT::duration{ 0 });
  auto const tempoDeltaT = sumDeltaT / numSelected;

  // juce::Logger::writeToLog ("averaging time deltas: "
  //                           + juce::String (_queueDeltaTs.size ()));
  // juce::Logger::writeToLog (
  //     "tempo: "
  //     + juce::String (
  //         std::abs (std::chrono::duration_cast<std::chrono::milliseconds> (
  //                       tempoDeltaT - deltaTAverage)
  //                       .count ())));

  setTempoDeltaT (tempoDeltaT);
}

}
