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

#include "Measure.hh"

#include <a3-motion-engine/tempo/TempoClock.hh>

namespace a3
{

int &
Measure::bar ()
{
  return _bar;
}

int &
Measure::beat ()
{
  return _beat;
}

int &
Measure::tick ()
{
  return _tick;
}

int const &
Measure::bar () const
{
  return _bar;
}

int const &
Measure::beat () const
{
  return _beat;
}

int const &
Measure::tick () const
{
  return _tick;
}

Measure &
Measure::operator+= (const Measure &rhs)
{
  _bar += rhs._bar;
  _beat += rhs._beat;
  _tick += rhs._tick;
  return *this;
}

Measure &
Measure::operator-= (Measure const &rhs)
{
  _bar -= rhs._bar;
  _beat -= rhs._beat;
  _tick -= rhs._tick;
  return *this;
}

Measure
operator+ (Measure lhs, const Measure &rhs)
{
  lhs += rhs;
  return lhs;
}

Measure
operator- (Measure lhs, Measure const &rhs)
{
  lhs -= rhs;
  return lhs;
}

bool
operator== (Measure const &lhs, Measure const &rhs)
{
  return lhs._bar == rhs._bar &&   //
         lhs._beat == rhs._beat && //
         lhs._tick == rhs._tick;
}

bool
operator!= (Measure const &lhs, Measure const &rhs)
{
  return !(lhs == rhs);
}

bool
operator< (const Measure &lhs, const Measure &rhs)
{
  return std::tie (lhs._bar, lhs._beat, lhs._tick)
         < std::tie (rhs._bar, rhs._beat, rhs._tick);
}

bool
operator<= (const Measure &lhs, const Measure &rhs)
{
  return lhs < rhs || lhs == rhs;
}

bool
operator> (const Measure &lhs, const Measure &rhs)
{
  return !(lhs < rhs || lhs == rhs);
}

bool
operator>= (const Measure &lhs, const Measure &rhs)
{
  return !(lhs < rhs);
}

int
Measure::convertToTicks (const Measure &measure, int beatsPerBar)
{
  return (measure.bar () * beatsPerBar + measure.beat ())
             * TempoClock::getTicksPerBeat ()
         + measure.tick ();
}

}
