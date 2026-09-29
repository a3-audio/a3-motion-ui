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

#include "ActionChain.hh"

namespace a3
{

void
ActionChain::pressed (int button, unsigned endCount)
{
  _running = button;
  _seen = endCount;
}

void
ActionChain::broken ()
{
  _running = -1;
}

std::optional<int>
ActionChain::accentEnded (unsigned endCount, AfterTable const &after)
{
  if (endCount == _seen)
    return std::nullopt;
  _seen = endCount;

  if (!juce::isPositiveAndBelow (_running, numActionButtons))
    return std::nullopt;

  auto const next = after[static_cast<size_t> (_running)];
  // Over either way: the next press -- the chain's own or a hand's -- says
  // which button runs from here.
  _running = -1;
  return next;
}

std::optional<int>
stepAfter (std::optional<int> after, int increment)
{
  // Nothing is the step before A1: seven places round.
  constexpr int places = numActionButtons + 1;
  auto const at = after ? *after + 1 : 0;
  auto const next = ((at + increment) % places + places) % places;
  if (next == 0)
    return std::nullopt;
  return next - 1;
}

juce::String
afterName (std::optional<int> after)
{
  if (!after || !juce::isPositiveAndBelow (*after, numActionButtons))
    return "--";
  return "A" + juce::String (*after + 1);
}

std::optional<int>
afterFromName (juce::String const &name)
{
  if (!name.startsWithChar ('A') || name.length () != 2)
    return std::nullopt;
  auto const number = name.substring (1).getIntValue ();
  if (number < 1 || number > numActionButtons)
    return std::nullopt;
  return number - 1;
}

bool
actionPressShowsItsPage (PadSource source, bool shift)
{
  switch (source)
    {
    case PadSource::Panel:
    case PadSource::PadsPage:
      return !shift;
    case PadSource::TransportKey:
    case PadSource::Scene:
    case PadSource::Chain:
      return false;
    }
  return false;
}

}
