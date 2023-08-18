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

#include "TempoEstimator.hh"

#include <algorithm>
#include <chrono>

#include <JuceHeader.h>

namespace a3
{

TempoEstimator::TapResult
TempoEstimator::tap ()
{
  auto timeTap = ClockT::now ();
  auto deltaT = timeTap - timeTapLast;
  timeTapLast = timeTap;

  if (deltaT > timeBetweenTapsMax)
    {
      juce::Logger::writeToLog ("flushing deltaTs queue");
      std::deque<ClockT::duration> queueEmpty;
      std::swap (_queueDeltaTs, queueEmpty);
    }
  else
    {
      if (_queueDeltaTs.size () == numTapsMax - 1)
        _queueDeltaTs.pop_front ();

      // juce::Logger::writeToLog (
      //     "recording deltaT: "
      //     + juce::String (
      //         std::chrono::duration_cast<std::chrono::milliseconds> (deltaT)
      //             .count ()));
      _queueDeltaTs.push_back (deltaT);

      if (_queueDeltaTs.size () >= numTapsMin)
        {
          estimateTempo ();
          return TapResult::TempoAvailable;
        }
    }

  return TapResult::TempoNotAvailable;
}

void
TempoEstimator::setTempoDeltaT (ClockT::duration deltaT)
{
  _tempoDeltaT = deltaT;
}

}
