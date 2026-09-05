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

#include <a3-motion-ui/components/ControllerLayout.hh>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

#include <algorithm>

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
  auto const titleRow = static_cast<int> (
      std::ceil (headerSize * rowHeightFactor));

  // The elevation section: its own title row, a graphic, then a 2x3 grid of
  // controls.
  auto const boxes = 3 * controlBoxHeightForFont (bodySize, knobDiameter);
  // The graphic takes the same share of the remainder it always did.
  auto const graphic = static_cast<int> (std::ceil (boxes * 0.34f / 0.66f));
  auto const elevation = titleRow + graphic + boxes;

  // The global section: three rows of knobs, the transport under them, then
  // three rows of buttons. It used to fit in whatever Elevation asked for,
  // back when it held one value — with the per-channel grid it can be the
  // taller of the two, and then the bar has to grow for it or the knobs get
  // squeezed to a few pixels.
  //
  // No title row and no row of channel numbers: the strip is not named any
  // more, and each grid column wears its channel's colour instead of being
  // numbered.
  //
  // The grid draws a fifth over the standard diameter and gives each row a
  // little more again — see layOutClipSettings().
  auto const gridRows
      = numChannelRows * static_cast<int> (std::ceil (knobDiameter * 1.4f));
  // Three rows of buttons plus the gaps between them — rec mode and clock
  // mode, menu and rec, then TAP across the width. Asking for one row's
  // worth is what collapsed the grid above them to a few pixels.
  auto const buttonRow
      = std::max (34, static_cast<int> (std::ceil (knobDiameter * 1.6f)));
  auto const buttons = 3 * buttonRow + 2 * std::max (2, buttonRow / 8);

  // The transport came down into the strip, in a frame of its own with a gap
  // above it. Asked for as its own block, or it takes the grid's room --
  // which is exactly what it did on the first try.
  auto const transport = buttonRow + 4 * std::max (2, buttonRow / 8);

  auto const global = gridRows + transport + buttons;

  // Motion: a title row, three rows of knobs and one of buttons. It used to
  // fit inside whatever the other two asked for, back when it had one row of
  // knobs; with the spin, the swell and the accent it can be the tallest of
  // the three, and a bar sized without it squeezes its rows.
  auto const motionRows = 4 * controlBoxHeightForFont (bodySize, knobDiameter);
  auto const motion = titleRow + motionRows + buttonRow;

  auto const sections = std::max (elevation, std::max (global, motion));

  // What comes back is a height for the whole bar, but everything above is
  // what a *section* needs. The bar spends its own chrome first: the header
  // row, the vertical padding twice (height/40) and the gap under the header
  // (height/50). Without allowing for it the sections were handed what they
  // asked for minus the chrome, and the global grid's knobs came out a few
  // pixels tall.
  //
  // The header is a ninth of the bar but never shorter than a fingertip, so
  // there are two answers rather than one fraction: at the sizes the device
  // ships with the ninth decides, and at the smallest font and pot the
  // fingertip does -- it is a fixed thirty-four pixels there, not a share, and
  // solving as though it were a share is what left the grid at six pixels.
  constexpr float otherChrome = 2.f / 40.f + 1.f / 50.f;

  auto const asAShare = static_cast<int> (
      std::ceil (static_cast<float> (sections)
                 / (1.f - 1.f / 9.f - otherChrome)));
  auto const asAFingertip = static_cast<int> (
      std::ceil (static_cast<float> (sections + fingertipSize)
                 / (1.f - otherChrome)));

  return std::max (asAShare, asAFingertip);
}

int
clipSettingsHeightWithin (int wanted, int screenHeight)
{
  return juce::jmin (
      wanted, static_cast<int> (static_cast<float> (screenHeight)
                                * maxClipSettingsScreenShare));
}

}
