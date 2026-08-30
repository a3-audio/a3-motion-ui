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


#include "ClipSettingsLayout.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>

namespace a3
{

namespace
{
constexpr int paddingH = 16;

// The global section takes half the bar — it holds a 4x3 grid of
// per-channel values plus the rec mode and three action buttons — and the
// clip's three sections share the other half.
int
clipSectionWidth (int rowWidth)
{
  return rowWidth / 2 / (numClipSettingsSections - 1);
}

/** The card's inner area, the same reduction every section makes. */
juce::Rectangle<int>
sectionContent (juce::Rectangle<int> card)
{
  return card.reduced (juce::jmax (2, card.getWidth () / 12), 4);
}
}

int
numControlsInSection (int sectionIndex)
{
  switch (sectionIndex)
    {
    case 0:
      return 2; // the shape itself, and the length of the next take
    case 1:
      return 6; // reach, clip-top, clip-bottom, mirror-south, flat, flat-elev
    case 2:
      return 4; // speed, direction, end-action, seam
    case 3:
      return 1; // rec mode — the global section's only encoder-ish value
    default:
      return 0;
    }
}

bool
tapAdvancesValue (int sectionIndex, int subIndex)
{
  if (sectionIndex == 1)
    return subIndex == 3 || subIndex == 4; // mirror-south, flat
  if (sectionIndex == 2)
    return subIndex == 1 || subIndex == 2; // direction, end-action
  if (sectionIndex == 3)
    return subIndex == 0; // rec mode

  return false;
}

juce::Rectangle<int>
textCell (juce::Rectangle<int> cell, int knobDiam)
{
  auto const boxH = juce::jmin (
      cell.getHeight (),
      static_cast<int> (static_cast<float> (knobDiam) * 2.2f));

  return juce::Rectangle<int> (cell.getWidth (), boxH)
      .withCentre (cell.getCentre ());
}

int
titleRowHeight (juce::Rectangle<int> content, float headerSize)
{
  auto const needed = static_cast<int> (headerSize * rowHeightFactor);

  return juce::jlimit (9, juce::jmax (9, content.getHeight () / 3), needed);
}

int
textRowHeight (juce::Rectangle<int> content, float size)
{
  auto const needed = static_cast<int> (size * rowHeightFactor);

  return juce::jlimit (10, juce::jmax (10, content.getHeight () / 2), needed);
}

ClipSettingsLayout
layOutClipSettings (juce::Rectangle<int> bounds, float headerSize,
                    float bodySize, float potSizeScale)
{
  ClipSettingsLayout out;

  // Two panels side by side, not one panel with an odd section on the end.
  out.globalBounds = bounds.removeFromRight (bounds.getWidth () / 2);
  out.clipBounds = bounds;

  auto const paddingV = juce::jmax (4, out.clipBounds.getHeight () / 40);
  auto const headerH = juce::jmax (18, out.clipBounds.getHeight () / 12);
  out.headerHeight = headerH;

  auto area = out.clipBounds.reduced (paddingH, paddingV);

  auto headerArea = area.removeFromTop (headerH);
  out.readout = headerArea.removeFromRight (headerArea.getWidth () / 2);
  out.slotLabel = headerArea;

  area.removeFromTop (juce::jmax (4, out.clipBounds.getHeight () / 50));

  auto const gap = juce::jmax (3, out.clipBounds.getWidth () / 200);
  auto const sectionW = area.getWidth () / (numClipSettingsSections - 1);

  auto const knobDiam = knobDiameterForFont (bodySize, potSizeScale);
  auto const cardW = sectionW - 2 * (gap / 2);
  auto const sectionContentW = cardW - 2 * juce::jmax (2, cardW / 12);
  auto const columnGap = juce::jmax (2, sectionContentW / 20);
  auto const controlBoxH = controlBoxHeightForFont (bodySize, knobDiam);

  out.metrics = ControlMetrics{
    knobDiam,
    sharedCaptionSize (bodySize, sectionContentW, columnGap, controlBoxH),
    sharedValueSize (bodySize, sectionContentW, columnGap, controlBoxH)
  };

  auto const &metrics = out.metrics;

  for (int i = 0; i < numClipSettingsSections - 1; ++i)
    out.sectionCards[static_cast<size_t> (i)]
        = area.removeFromLeft (sectionW).reduced (gap / 2, 0);

  auto globalArea = out.globalBounds.reduced (paddingH, paddingV);
  globalArea.removeFromTop (headerH);
  out.sectionCards[3] = globalArea.reduced (gap / 2, 0);

  // ── Shape ────────────────────────────────────────────────────────────
  {
    auto content = sectionContent (out.sectionCards[0]);
    out.sectionLabels[0]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    auto const lengthArea = content.removeFromBottom (
        2 * textRowHeight (content, metrics.valueSize));
    out.trajectoryName
        = content.removeFromBottom (textRowHeight (content, metrics.valueSize));
    out.trajectoryIcon = content;

    // Sub 0 is the pattern in the slot: its hit area is the pictogram plus
    // the name underneath, which is what a finger aims at.
    out.controls[0] = { out.trajectoryIcon.getUnion (out.trajectoryName),
                        lengthArea };
  }

  // ── Elevation ────────────────────────────────────────────────────────
  {
    auto content = sectionContent (out.sectionCards[1]);
    out.sectionLabels[1]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    auto const gapV0 = juce::jmax (2, content.getHeight () / 20);
    out.elevationGraphic = content.removeFromTop (
        static_cast<int> (content.getHeight () * 0.34f));
    content.removeFromTop (gapV0);

    auto const gapV = juce::jmax (2, content.getHeight () / 30);
    auto const rowH = (content.getHeight () - 2 * gapV) / 3;
    auto row1 = content.removeFromTop (rowH);
    content.removeFromTop (gapV);
    auto row2 = content.removeFromTop (rowH);
    content.removeFromTop (gapV);
    auto row3 = content;

    auto const gapH = juce::jmax (2, content.getWidth () / 20);

    auto const reachArea = row1.removeFromLeft (row1.getWidth () / 2 - gapH / 2);
    row1.removeFromLeft (gapH);
    auto const mirrorArea = row1;

    auto const clipTopArea
        = row2.removeFromLeft (row2.getWidth () / 2 - gapH / 2);
    row2.removeFromLeft (gapH);
    auto const clipBottomArea = row2;

    auto const flatArea = row3.removeFromLeft (row3.getWidth () / 2 - gapH / 2);
    row3.removeFromLeft (gapH);
    auto const flatElevationArea = row3;

    // By sub-index, not by row: mirror-south is 3 but sits on the first row.
    out.controls[1] = {
      textCell (reachArea, metrics.knobDiam),
      textCell (clipTopArea, metrics.knobDiam),
      textCell (clipBottomArea, metrics.knobDiam),
      textCell (mirrorArea, metrics.knobDiam),
      textCell (flatArea, metrics.knobDiam),
      textCell (flatElevationArea, metrics.knobDiam),
    };
  }

  // ── Motion ───────────────────────────────────────────────────────────
  {
    auto content = sectionContent (out.sectionCards[2]);
    out.sectionLabels[2]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    auto const gapH = juce::jmax (2, content.getWidth () / 20);
    auto const colW = (content.getWidth () - 3 * gapH) / 4;
    auto const speedArea = content.removeFromLeft (colW);
    content.removeFromLeft (gapH);
    auto const directionArea = content.removeFromLeft (colW);
    content.removeFromLeft (gapH);
    auto const endActionArea = content.removeFromLeft (colW);
    content.removeFromLeft (gapH);
    auto const seamArea = content;

    out.controls[2] = {
      textCell (speedArea, metrics.knobDiam),
      textCell (directionArea, metrics.knobDiam),
      textCell (endActionArea, metrics.knobDiam),
      textCell (seamArea, metrics.knobDiam),
    };
  }

  // ── Global section ───────────────────────────────────────────────────
  {
    auto content = sectionContent (out.sectionCards[3]);
    out.sectionLabels[3]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    // The grid on the left, the rec mode and the buttons in a strip on the
    // right. A third of the width is enough for two buttons side by side and
    // leaves the four channel columns room to stay square-ish.
    auto strip = content.removeFromRight (content.getWidth () / 3);
    auto const stripGap = juce::jmax (2, content.getWidth () / 40);
    content.removeFromRight (stripGap);

    // ── the 4 x 3 grid ────────────────────────────────────────────────
    {
      auto grid = content;
      auto const labelH = textRowHeight (grid, metrics.captionSize);
      auto const headerRow = grid.removeFromTop (labelH);

      // A narrow gutter down the left for the row captions: freq, Q, 3d
      // said once each rather than twelve times.
      auto const gutterW = juce::jmax (labelH, grid.getWidth () / 8);
      auto gutter = grid.removeFromLeft (gutterW);
      auto columns = grid;
      auto headerColumns = headerRow;
      headerColumns.removeFromLeft (gutterW);

      auto const colW = columns.getWidth () / numChannelColumns;
      auto const rowH = columns.getHeight () / numChannelRows;

      for (int row = 0; row < numChannelRows; ++row)
        out.channelRowLabels[static_cast<size_t> (row)]
            = gutter.removeFromTop (rowH);

      for (int col = 0; col < numChannelColumns; ++col)
        {
          auto const c = static_cast<size_t> (col);
          out.channelLabels[c] = headerColumns.removeFromLeft (colW);

          auto column = columns.removeFromLeft (colW);
          for (int row = 0; row < numChannelRows; ++row)
            out.channelGrid[c][static_cast<size_t> (row)]
                = column.removeFromTop (rowH).reduced (2);
        }
    }

    // ── rec mode and the action buttons ───────────────────────────────
    auto buttons = strip.removeFromBottom (strip.getHeight () * 5 / 9);
    out.controls[3] = { textCell (strip, metrics.knobDiam) };

    auto const gap = juce::jmax (2, buttons.getHeight () / 20);
    auto topRow = buttons.removeFromTop ((buttons.getHeight () - gap) / 2);
    buttons.removeFromTop (gap);

    auto const gapH = juce::jmax (2, topRow.getWidth () / 20);
    out.menuButton = topRow.removeFromLeft ((topRow.getWidth () - gapH) / 2);
    topRow.removeFromLeft (gapH);
    out.recButton = topRow;

    out.tapButton = buttons;
  }

  return out;
}

}
