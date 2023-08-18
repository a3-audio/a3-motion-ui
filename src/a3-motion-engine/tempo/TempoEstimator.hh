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
#include <deque>

namespace a3
{

class TempoEstimator
{
public:
  enum class TapResult
  {
    TempoAvailable,
    TempoNotAvailable
  };

  virtual ~TempoEstimator (){};

  TapResult tap ();
  virtual void estimateTempo () = 0;

  float getTempoBPM () const;

protected:
  using ClockT = std::chrono::high_resolution_clock;
  static auto constexpr numTapsMin = 3;
  static auto constexpr numTapsMax = 16;
  static auto constexpr timeBetweenTapsMax = std::chrono::seconds (2);

  void setTempoDeltaT (ClockT::duration deltaT);

  std::deque<ClockT::duration> _queueDeltaTs;

private:
  ClockT::time_point timeTapLast;
  ClockT::duration _tempoDeltaT;
};

}
