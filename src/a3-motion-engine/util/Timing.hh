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
#include <string>
#include <vector>

namespace a3
{

template <typename ClockT>
class ScopedTimer;

template <typename ClockT = std::chrono::high_resolution_clock>
class Timings
{
public:
  struct Measurement
  {
    typename std::chrono::time_point<ClockT>::duration duration;
    std::string tag;
  };
  using ContainerT = std::vector<Measurement>;

  ContainerT const &
  get () const
  {
    return measurements;
  }

private:
  friend class ScopedTimer<ClockT>;
  ContainerT measurements;
};

template <typename ClockT = std::chrono::high_resolution_clock>
class ScopedTimer
{
public:
  ScopedTimer (Timings<ClockT> &timings, std::string tag = "")
      : timings (timings), tag (tag)
  {
    t0 = ClockT::now ();
  }
  ~ScopedTimer ()
  {
    auto duration = ClockT::now () - t0;
    timings.measurements.push_back ({ duration, tag });
  }

private:
  std::string tag;
  std::chrono::time_point<ClockT> t0;
  Timings<ClockT> &timings;
};

}
