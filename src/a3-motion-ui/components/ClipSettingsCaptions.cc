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

namespace a3
{

namespace
{
// A caption row is drawn 1.25 times the font's height (labelRowHeight) and is
// given at most half of its control box, so the box allows 0.4 times its own
// height as a font size.
constexpr float captionRowShare = 0.5f / 1.25f;

// Below this a caption is no longer a caption. If the bar is ever this small
// the layout is wrong, not the font size.
constexpr float smallestReadable = 7.f;
}

float
sharedCaptionSize (float baseSize, int sectionContentWidth, int columnGap,
                   int controlBoxHeight)
{
  auto size = baseSize;

  for (auto const &entry : captionTable)
    {
      auto const columnWidth = static_cast<float> (
          (sectionContentWidth - (entry.columns - 1) * columnGap)
          / entry.columns);
      auto const width = juce::GlyphArrangement::getStringWidth (
          juce::Font (juce::FontOptions (baseSize)), entry.text);

      if (width > columnWidth && width > 0.f)
        size = juce::jmin (size, baseSize * columnWidth / width);
    }

  size = juce::jmin (size,
                     static_cast<float> (controlBoxHeight) * captionRowShare);

  return juce::jmax (smallestReadable, size);
}

}
