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

#include <a3-motion-ui/io/LedLoad.hh>

#include <algorithm>

namespace a3
{

LedLoad::LedLoad (std::size_t count) : _channels (count, 0u)
{
  _peak = estimateMilliamps ();
}

void
LedLoad::set (std::size_t led, juce::Colour colour)
{
  if (led >= _channels.size ())
    return;

  auto const sum = static_cast<std::uint32_t> (colour.getRed ())
                   + colour.getGreen () + colour.getBlue ();
  _total = _total - _channels[led] + sum;
  _channels[led] = sum;
  _peak = std::max (_peak, estimateMilliamps ());
}

std::uint32_t
LedLoad::estimateMilliamps () const
{
  auto const chips = static_cast<std::uint64_t> (_channels.size ()) * ledMaIdle;
  return static_cast<std::uint32_t> (chips
                                     + _total * ledMaPerChannelAtFull / 255u);
}

std::uint32_t
LedLoad::peakMilliamps () const
{
  return _peak;
}

bool
LedLoad::takeNewPeak (std::uint32_t step)
{
  if (_peak < _reported + step)
    return false;

  _reported = _peak;
  return true;
}

bool
LedLoad::takeBudgetCrossing ()
{
  auto const over = estimateMilliamps () > firmwareLedBudgetMa;
  auto const crossed = over && !_wasOver;
  _wasOver = over;
  return crossed;
}

}
