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

#include "KnobLanes.hh"

#include <cmath>
#include <limits>

namespace a3
{

namespace
{
constexpr float unwritten = std::numeric_limits<float>::quiet_NaN ();

bool
isWritten (float value)
{
  return !std::isnan (value);
}
}

KnobLane::KnobLane (long long ticks)
    : _values (static_cast<std::size_t> (ticks > 0 ? ticks : 0), unwritten)
{
}

bool
KnobLane::empty () const
{
  for (auto const value : _values)
    if (isWritten (value))
      return false;
  return true;
}

void
KnobLane::write (long long tick, float value)
{
  if (_values.empty () || !std::isfinite (value))
    return;

  auto const size = static_cast<long long> (_values.size ());
  _values[static_cast<std::size_t> (((tick % size) + size) % size)] = value;
}

std::optional<float>
KnobLane::at (double tick) const
{
  if (_values.empty ())
    return {};

  auto const size = static_cast<long long> (_values.size ());
  auto const here
      = ((static_cast<long long> (std::floor (tick)) % size) + size) % size;

  // Backwards from here, round the end: the last value written holds.
  for (long long back = 0; back < size; ++back)
    {
      auto const value = _values[static_cast<std::size_t> (
          ((here - back) % size + size) % size)];
      if (isWritten (value))
        return value;
    }
  return {};
}

std::vector<std::pair<int, float> >
KnobLane::changePoints () const
{
  std::vector<std::pair<int, float> > points;

  // The value each written tick changes from is the one written before it,
  // round the end -- which is what at() would give the tick before.
  std::optional<float> previous;
  for (auto i = _values.size (); i-- > 0;)
    if (isWritten (_values[i]))
      {
        previous = _values[i];
        break;
      }

  for (std::size_t i = 0; i < _values.size (); ++i)
    {
      auto const value = _values[i];
      if (!isWritten (value))
        continue;
      if (!previous || value != *previous)
        points.emplace_back (static_cast<int> (i), value);
      previous = value;
    }

  // A lane that never changes is one point, or it would say nothing.
  if (points.empty () && previous)
    for (std::size_t i = 0; i < _values.size (); ++i)
      if (isWritten (_values[i]))
        {
          points.emplace_back (static_cast<int> (i), _values[i]);
          break;
        }

  return points;
}

KnobLane
KnobLane::fromChangePoints (std::vector<std::pair<int, float> > const &points,
                            long long ticks)
{
  KnobLane lane (ticks);
  for (auto const &[tick, value] : points)
    if (tick >= 0 && tick < ticks)
      lane.write (tick, value);
  return lane;
}

void
KnobRecorder::recordTick (KnobLane &lane, RecMode mode, bool held,
                          float value, long long ticksNow, long long lapTicks)
{
  if (held)
    _hasTouched = true;
  else if (_wasHeld)
    _ticksAtLift = ticksNow;
  _wasHeld = held;

  if (shouldWriteTick (mode,
                       { held, _hasTouched, _ticksAtLift, ticksNow, lapTicks }))
    lane.write (lapTicks > 0 ? ticksNow % lapTicks : ticksNow, value);
}

}
