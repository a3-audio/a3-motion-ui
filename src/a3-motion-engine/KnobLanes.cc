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

constexpr long long wordBits = 64;

std::size_t
wordsFor (std::size_t bits)
{
  return (bits + wordBits - 1) / wordBits;
}

/** The highest set bit of `word` at or below `bit`; -1 for none. */
int
highestSetAtOrBelow (std::uint64_t word, int bit)
{
  auto const mask = bit >= 63 ? ~std::uint64_t{ 0 }
                              : (std::uint64_t{ 1 } << (bit + 1)) - 1;
  auto const kept = word & mask;
  return kept == 0 ? -1 : 63 - __builtin_clzll (kept);
}
}

KnobLane::KnobLane (long long ticks)
    : _values (static_cast<std::size_t> (ticks > 0 ? ticks : 0), unwritten),
      _written (wordsFor (_values.size ()), 0),
      _wordsWritten (wordsFor (_written.size ()), 0)
{
}

long long
KnobLane::lastWrittenAtOrBefore (long long tick) const
{
  if (tick < 0)
    return -1;

  auto word = tick / wordBits;
  if (auto const bit = highestSetAtOrBelow (
          _written[static_cast<std::size_t> (word)],
          static_cast<int> (tick % wordBits));
      bit >= 0)
    return word * wordBits + bit;

  // The words before, through the index of which ones hold anything.
  for (auto before = word - 1; before >= 0;)
    {
      auto const group = before / wordBits;
      auto const found = highestSetAtOrBelow (
          _wordsWritten[static_cast<std::size_t> (group)],
          static_cast<int> (before % wordBits));
      if (found >= 0)
        {
          word = group * wordBits + found;
          return word * wordBits
                 + highestSetAtOrBelow (
                     _written[static_cast<std::size_t> (word)], 63);
        }
      before = group * wordBits - 1;
    }
  return -1;
}

bool
KnobLane::empty () const
{
  for (auto const words : _wordsWritten)
    if (words != 0)
      return false;
  return true;
}

void
KnobLane::write (long long tick, float value)
{
  if (_values.empty () || !std::isfinite (value))
    return;

  auto const size = static_cast<long long> (_values.size ());
  auto const at = ((tick % size) + size) % size;
  _values[static_cast<std::size_t> (at)] = value;
  auto const word = at / wordBits;
  _written[static_cast<std::size_t> (word)]
      |= std::uint64_t{ 1 } << (at % wordBits);
  _wordsWritten[static_cast<std::size_t> (word / wordBits)]
      |= std::uint64_t{ 1 } << (word % wordBits);
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
  auto written = lastWrittenAtOrBefore (here);
  if (written < 0)
    written = lastWrittenAtOrBefore (size - 1);
  if (written < 0)
    return {};
  return _values[static_cast<std::size_t> (written)];
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

bool
KnobRecorder::recordTick (KnobLane &lane, RecMode mode, bool held,
                          float value, long long ticksNow, long long lapTicks)
{
  if (held)
    _hasTouched = true;
  else if (_wasHeld)
    _ticksAtLift = ticksNow;
  _wasHeld = held;

  if (!shouldWriteTick (mode,
                        { held, _hasTouched, _ticksAtLift, ticksNow, lapTicks }))
    return false;

  lane.write (lapTicks > 0 ? ticksNow % lapTicks : ticksNow, value);
  return true;
}

}
