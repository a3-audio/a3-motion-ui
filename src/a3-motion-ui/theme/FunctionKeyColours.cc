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

#include "FunctionKeyColours.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>

namespace a3
{

juce::Colour
functionKeyColour (FunctionKey key, FunctionKeyLook const &look)
{
  switch (key)
    {
    case FunctionKey::ClockMode:
      // Always coloured: which clock is running is never "nothing".
      return Colours::clockMode (look.clockMode);

    case FunctionKey::Record:
      // Also always, and this is the point of it — recording writes over what
      // you cannot get back, so armed has to look like something too.
      return look.recording ? toColour (theme ().danger)
                            : toColour (theme ().warning);

    case FunctionKey::Shift:
      return look.shiftHeld ? toColour (theme ().accent) : juce::Colour{};

    case FunctionKey::Tap:
      return (look.tapPressed || look.tapBeat) ? toColour (theme ().accent)
                                               : juce::Colour{};

    case FunctionKey::Menu:
    case FunctionKey::RecMode:
      // These say what they do with a word. A colour on them would be one
      // that means nothing, and every colour that means nothing makes the
      // ones that mean something harder to read.
      return {};
    }

  return {};
}

}
