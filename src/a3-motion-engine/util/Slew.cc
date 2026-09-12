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

#include "Slew.hh"

#include <cmath>

namespace a3
{

float
slewTowards (float current, float target, double elapsedMillis,
             double rampMillis)
{
  // Smoothing switched off, or asked for backwards. Either way the caller
  // wants the value, not a ramp.
  if (rampMillis <= 0.)
    return target;

  // Two calls inside one clock resolution, or a clock that went backwards.
  // Nothing has elapsed, so nothing moves -- the next call will carry it.
  if (!(elapsedMillis > 0.))
    return current;

  auto const step
      = static_cast<float> (elapsedMillis / rampMillis);
  auto const distance = target - current;

  // Within one step: land on it exactly. This is the branch that matters --
  // see Slew.hh on why a value at rest has to be the set value to the bit.
  if (std::abs (distance) <= step)
    return target;

  return current + (distance > 0.f ? step : -step);
}

}
