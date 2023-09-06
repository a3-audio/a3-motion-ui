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

#include <tuple>

#include <a3-motion-engine/util/Types.hh>

namespace a3
{

class Measure
{
public:
  Measure () : _bar (0), _beat (0), _tick (0) {}
  Measure (int bar, int beat, int tick)
      : _bar (bar), _beat (beat), _tick (tick)
  {
  }

  int &bar ();
  int &beat ();
  int &tick ();

  int const &bar () const;
  int const &beat () const;
  int const &tick () const;

  Measure &operator+= (const Measure &rhs);
  Measure &operator-= (Measure const &rhs);
  friend Measure operator+ (Measure lhs, const Measure &rhs);
  friend Measure operator- (Measure lhs, Measure const &rhs);
  friend bool operator== (Measure const &lhs, Measure const &rhs);
  friend bool operator!= (Measure const &lhs, Measure const &rhs);
  friend bool operator< (const Measure &lhs, const Measure &rhs);
  friend bool operator<= (const Measure &lhs, const Measure &rhs);
  friend bool operator> (const Measure &lhs, const Measure &rhs);
  friend bool operator>= (const Measure &lhs, const Measure &rhs);

  static int convertToTicks (const Measure &measure, int beatsPerBar);

private:
  int _bar;
  int _beat;
  int _tick;
};

}
