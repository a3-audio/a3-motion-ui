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

#include "TransportLook.hh"

#include <a3-motion-ui/theme/ThemeColours.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

juce::Colour
transportColour (TransportKey key)
{
  switch (key)
    {
    case TransportKey::Record:
      return toColour (theme ().danger);

    case TransportKey::Stop:
      return toColour (theme ().danger);

    case TransportKey::PlayPause:
      // Green, running or not. The colour says which key this is, and a key
      // that changes colour with its state is one you have to look at twice --
      // once to find it and once to read it. Whether the clip is running is
      // said by the shape (triangle or bars) and by the key's own ground.
      return toColour (theme ().accent);

    case TransportKey::Action:
      return toColour (theme ().highlight);
    }

  return {};
}

TransportKey
transportKeyForPad (PadFunction function)
{
  switch (function)
    {
    case PadFunction::Stop: return TransportKey::Stop;
    case PadFunction::Action: return TransportKey::Action;
    case PadFunction::PlayPause:
    case PadFunction::Settings: break;
    }

  return TransportKey::PlayPause;
}

bool
hasTransportGlyph (PadFunction function)
{
  return function != PadFunction::Settings;
}

void
drawTransportGlyph (juce::Graphics &g, juce::Rectangle<float> area,
                    TransportKey key, bool playing)
{
  switch (key)
    {
    case TransportKey::Record:
      g.fillEllipse (area);
      return;

    case TransportKey::Stop:
      g.fillRect (area);
      return;

    case TransportKey::PlayPause:
      if (playing)
        {
          auto const barW = area.getWidth () * 0.34f;
          g.fillRect (area.withWidth (barW));
          g.fillRect (area.withWidth (barW).withRightX (area.getRight ()));
        }
      else
        {
          juce::Path play;
          play.addTriangle (area.getX (), area.getY (), area.getX (),
                            area.getBottom (), area.getRight (),
                            area.getCentreY ());
          g.fillPath (play);
        }
      return;

    case TransportKey::Action:
      {
        // Sized to the box it is given rather than to the bar's body size:
        // this is a mark like the other three, not a caption.
        g.setFont (juce::Font (area.getHeight (), juce::Font::bold));
        g.drawFittedText ("A", area.toNearestInt (),
                          juce::Justification::centred, 1);
        return;
      }
    }
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
padFunctionColour (PadFunction function)
{
  if (!hasTransportGlyph (function))
    return {};

  return transportColour (transportKeyForPad (function));
}

}
