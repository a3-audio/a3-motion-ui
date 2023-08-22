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
TempoEstimator::tap (juce::int64 timeMicros)
{
  auto deltaT = timeMicros - timeTapLastMicros;
  timeTapLastMicros = timeMicros;

  if (deltaT > std::chrono::duration_cast<std::chrono::microseconds> (
                   timeBetweenTapsMax)
                   .count ())
    {
      // juce::Logger::writeToLog ("flushing tap time queue");
      std::deque<ClockT::time_point> queueEmpty;
      std::swap (_queueTapTimes, queueEmpty);
    }
  else if (_queueTapTimes.size () == numTapsMax)
    _queueTapTimes.pop_front ();

  _queueTapTimes.push_back (ClockT::time_point ()
                            + std::chrono::microseconds (timeMicros));

  if (_queueTapTimes.size () >= numTapsMin)
    {
      estimateTempo ();
      return TapResult::TempoAvailable;
    }

  return TapResult::TempoNotAvailable;
}

std::vector<TempoEstimator::ClockT::time_point>
TempoEstimator::getTapTimes ()
{
  std::vector<ClockT::time_point> tapTimes;
  tapTimes.reserve (_queueTapTimes.size ());
  std::copy (_queueTapTimes.begin (), _queueTapTimes.end (),
             std::back_inserter (tapTimes));
  return tapTimes;
}

std::vector<TempoEstimator::ClockT::duration>
TempoEstimator::getTapTimeDeltas ()
{
  auto tapTimeDeltas
      = std::vector<ClockT::duration> (_queueTapTimes.size () - 1);
  auto timeLast = ClockT::time_point ();
  for (auto index = 0u; index < _queueTapTimes.size (); ++index)
    {
      if (index == 0)
        {
          timeLast = _queueTapTimes[index];
          continue;
        }

      tapTimeDeltas[index - 1] = _queueTapTimes[index] - timeLast;
      timeLast = _queueTapTimes[index];
    }

  return tapTimeDeltas;
}

void
TempoEstimator::setTempoDeltaT (ClockT::duration deltaT)
{
  _tempoDeltaT = deltaT;
}

TempoEstimator::ClockT::duration
TempoEstimator::getTempoDeltaT () const
{
  return _tempoDeltaT;
}

float
TempoEstimator::getTempoBPM () const
{
  auto const tempoDeltaTMicros
      = std::chrono::duration_cast<std::chrono::microseconds> (
            getTempoDeltaT ())
            .count ();
  return float (60.0 * 1e6 / tempoDeltaTMicros);
}

}
