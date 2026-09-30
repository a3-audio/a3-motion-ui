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

#include "FittedFont.hh"

#include <JuceHeader.h>

namespace a3
{

float
fittedFontHeight (float wanted, float cap)
{
  return juce::jmax (1.f, juce::jmin (cap, wanted));
}

float
heightToFitWidth (juce::Font const &font, juce::String const &text,
                  float width)
{
  auto const needed = juce::GlyphArrangement::getStringWidth (font, text);
  if (needed <= width)
    return font.getHeight ();

  // Glyph widths grow with the height, near enough in proportion; a hair
  // under the share keeps rounding from tipping it back over.
  constexpr float roundingMargin = 0.98f;
  return juce::jmax (1.f, font.getHeight () * width / needed * roundingMargin);
}

}
