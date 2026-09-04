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

#include "TransportColours.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

juce::Colour
transportColour (TransportKey key, bool playing)
{
  switch (key)
    {
    case TransportKey::Record:
      return toColour (theme ().danger);

    case TransportKey::Stop:
      return toColour (theme ().danger);

    case TransportKey::PlayPause:
      return playing ? toColour (theme ().accent) : toColour (theme ().danger);

    case TransportKey::Action:
      return toColour (theme ().highlight);
    }

  return {};
}

float
relativeLuminance (juce::Colour colour)
{
  auto const channel = [] (float v) {
    return v <= 0.03928f ? v / 12.92f
                         : std::pow ((v + 0.055f) / 1.055f, 2.4f);
  };

  return 0.2126f * channel (colour.getFloatRed ())
         + 0.7152f * channel (colour.getFloatGreen ())
         + 0.0722f * channel (colour.getFloatBlue ());
}

float
contrastRatio (juce::Colour a, juce::Colour b)
{
  auto const la = relativeLuminance (a);
  auto const lb = relativeLuminance (b);
  auto const hi = std::max (la, lb);
  auto const lo = std::min (la, lb);

  return (hi + 0.05f) / (lo + 0.05f);
}

juce::Colour
padFunctionColour (PadFunction function, bool playing)
{
  switch (function)
    {
    case PadFunction::PlayPause:
      return transportColour (TransportKey::PlayPause, playing);
    case PadFunction::Stop:
      return transportColour (TransportKey::Stop);
    case PadFunction::Action:
      return transportColour (TransportKey::Action);
    case PadFunction::Settings:
      return {};
    }

  return {};
}

}
