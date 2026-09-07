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

#include "ButtonLedColours.hh"

namespace a3
{

LedColour
buttonLedColour (juce::var const &buttonLedsConfig, juce::String const &name)
{
  if (!buttonLedsConfig.hasProperty (name))
    return ledColourUnassigned;

  auto const entry = buttonLedsConfig[juce::Identifier (name)];

  // All three or none: a missing channel would read as 0 and quietly darken
  // the button, which looks like broken hardware rather than a config gap.
  if (!entry.hasProperty ("r") || !entry.hasProperty ("g")
      || !entry.hasProperty ("b"))
    return ledColourUnassigned;

  auto const channel = [&entry] (char const *key) {
    return juce::jlimit (0, 255,
                         static_cast<int> (entry[juce::Identifier (key)]));
  };

  return LedColour{ channel ("r"), channel ("g"), channel ("b") };
}


juce::Colour
toColour (LedColour const &colour)
{
  return juce::Colour (static_cast<juce::uint8> (colour.r),
                       static_cast<juce::uint8> (colour.g),
                       static_cast<juce::uint8> (colour.b));
}


LedColour
buttonLedIdleColour (juce::var const &buttonLedsConfig)
{
  return buttonLedColour (buttonLedsConfig, "idle");
}


juce::Colour
ledColour (juce::Colour colour)
{
  // No hue, nothing to bring out: white stays white, and a grey that was
  // meant as a grey is not turned into a colour it never had.
  if (colour.getSaturation () <= 0.f)
    return colour;

  if (colour.getSaturation () >= ledMinSaturation)
    return colour;

  return colour.withSaturation (ledMinSaturation);
}

}
