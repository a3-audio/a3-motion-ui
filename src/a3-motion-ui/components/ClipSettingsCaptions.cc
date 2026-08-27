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

#include "ClipSettingsCaptions.hh"

#include <cmath>

namespace a3
{

namespace
{
// Below this a caption is no longer a caption. If the bar is ever this small
// the layout is wrong, not the font size.
constexpr float smallestReadable = 7.f;

/** The largest size at which every entry still fits both the column it is
 *  drawn in and the row's share of the control box. */
template <std::size_t count>
float
fittedSize (float baseSize, TextEntry const (&entries)[count],
            int sectionContentWidth, int columnGap, int controlBoxHeight,
            float rowShare)
{
  auto size = baseSize;

  for (auto const &entry : entries)
    {
      auto const columnWidth = static_cast<float> (
          (sectionContentWidth - (entry.columns - 1) * columnGap)
          / entry.columns);
      auto const width = juce::GlyphArrangement::getStringWidth (
          juce::Font (juce::FontOptions (baseSize)), entry.text);

      if (width > columnWidth && width > 0.f)
        size = juce::jmin (size, baseSize * columnWidth / width);
    }

  // Only a floor now, not a ceiling: the box is built to hold this text
  // (controlBoxHeightForFont), so capping the font against the box would put
  // the old inversion straight back. It still binds where the bar has been
  // clamped to its share of the screen, which is the one place it should.
  size = juce::jmin (size, static_cast<float> (controlBoxHeight) * rowShare
                               / rowHeightFactor);

  return juce::jmax (smallestReadable, size);
}
}

float
sharedCaptionSize (float baseSize, int sectionContentWidth, int columnGap,
                   int controlBoxHeight)
{
  return fittedSize (baseSize, captionTable, sectionContentWidth, columnGap,
                     controlBoxHeight, captionRowShare);
}

float
sharedValueSize (float baseSize, int sectionContentWidth, int columnGap,
                 int controlBoxHeight)
{
  return fittedSize (baseSize, valueTable, sectionContentWidth, columnGap,
                     controlBoxHeight, valueRowShare);
}

int
knobDiameterForFont (float bodySize, float potSizeScale)
{
  // 1.5 x the body size is what the old layout produced at the shipped
  // settings, where the knob came out of the row height rather than the font.
  constexpr float knobFontFactor = 1.5f;

  return juce::jmax (
      10, static_cast<int> (bodySize * knobFontFactor * potSizeScale));
}

int
controlBoxHeightForFont (float bodySize, int knobDiameter)
{
  // A value row and a caption row, both at the body size, plus the knob.
  auto const textRows = 2.f * bodySize * rowHeightFactor;

  return knobDiameter + static_cast<int> (std::ceil (textRows));
}

int
clipSettingsPreferredHeight (float headerSize, float bodySize,
                             int knobDiameter)
{
  // The elevation section is the tallest: its own title row, a graphic, then
  // a 2x3 grid of controls. Everything else fits in less, so this is what the
  // bar has to be able to show.
  auto const titleRow = static_cast<int> (
      std::ceil (headerSize * rowHeightFactor));
  auto const boxes = 3 * controlBoxHeightForFont (bodySize, knobDiameter);

  // The graphic takes the same share of the remainder it always did.
  auto const graphic = static_cast<int> (std::ceil (boxes * 0.34f / 0.66f));

  return titleRow + graphic + boxes;
}

int
clipSettingsHeightWithin (int wanted, int screenHeight)
{
  return juce::jmin (
      wanted, static_cast<int> (static_cast<float> (screenHeight)
                                * maxClipSettingsScreenShare));
}

}
