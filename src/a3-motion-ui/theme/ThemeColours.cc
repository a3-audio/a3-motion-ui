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

#include "ThemeColours.hh"

namespace a3
{

juce::Colour
toColour (ThemeColour const &colour)
{
  return juce::Colour (static_cast<juce::uint8> (colour.r),
                       static_cast<juce::uint8> (colour.g),
                       static_cast<juce::uint8> (colour.b));
}

juce::Colour
toColour (ThemeColour const &colour, float alpha)
{
  return toColour (colour).withAlpha (alpha);
}

namespace Colours
{

juce::Colour
clockMode (int mode)
{
  if (mode == 1)
    return toColour (theme ().warning);
  if (mode == 2)
    return toColour (theme ().notice);

  return toColour (theme ().accent);
}

juce::Colour
background ()
{
  return toColour (theme ().background);
}

juce::Colour
statusBar ()
{
  return background ().withLightness (0.4f);
}

}

}
