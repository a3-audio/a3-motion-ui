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
    return _measurements;
  }

private:
  friend class ScopedTimer<ClockT>;
  ContainerT _measurements;
};

template <typename ClockT = std::chrono::high_resolution_clock>
class ScopedTimer
{
public:
  ScopedTimer (Timings<ClockT> &timings, std::string tag = "")
      : _tag (tag), _timings (timings)
  {
    _t0 = ClockT::now ();
  }
  ~ScopedTimer ()
  {
    auto duration = ClockT::now () - _t0;
    _timings._measurements.push_back ({ duration, _tag });
  }

private:
  std::string _tag;
  std::chrono::time_point<ClockT> _t0;
  Timings<ClockT> &_timings;
};

}
