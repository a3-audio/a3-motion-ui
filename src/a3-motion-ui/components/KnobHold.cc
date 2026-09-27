/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include "KnobHold.hh"

namespace a3
{

namespace
{
std::size_t
slot (Knob knob)
{
  return static_cast<std::size_t> (knob);
}
}

void
KnobHold::press (Knob knob)
{
  _pressed[slot (knob)] = true;
}

void
KnobHold::release (Knob knob)
{
  _pressed[slot (knob)] = false;
}

void
KnobHold::nudge (Knob knob, double nowMs)
{
  _nudgedAt[slot (knob)] = nowMs;
}

bool
KnobHold::isHeld (Knob knob, double nowMs) const
{
  if (_pressed[slot (knob)])
    return true;

  auto const &nudged = _nudgedAt[slot (knob)];
  return nudged && nowMs - *nudged < encoderHoldMs;
}

}
