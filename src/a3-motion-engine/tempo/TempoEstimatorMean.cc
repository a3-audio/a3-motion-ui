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

#include "TempoEstimatorMean.hh"

#include <numeric>

#include <JuceHeader.h>

namespace a3
{

TempoEstimatorMean::TempoEstimatorMean () {}

void
TempoEstimatorMean::estimateTempo ()
{
  jassert (_queueDeltaTs.size () > 0);

  ClockT::duration sumDeltaT{ 0 };
  for (auto &deltaT : _queueDeltaTs)
    {
      sumDeltaT += deltaT;
    }
  auto tempoDeltaT = sumDeltaT / _queueDeltaTs.size ();

  setTempoDeltaT (tempoDeltaT);
}

}
