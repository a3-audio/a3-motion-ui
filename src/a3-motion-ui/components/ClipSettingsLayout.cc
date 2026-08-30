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

// The global section takes a quarter of the bar and the clip's three
// sections share the rest. It had half while its grid was spread across the
// whole width; capped to what the knobs need, the grid fits in a quarter and
// the clip's sections get the room back.
int
clipSectionWidth (int rowWidth)
{
  return rowWidth * 3 / 4 / (numClipSettingsSections - 1);
}

/** The card's inner area, the same reduction every section makes. A twelfth
 *  of the width was a wide margin on a narrow section and an enormous one on
 *  the global section, which is half the bar. */
juce::Rectangle<int>
sectionContent (juce::Rectangle<int> card)
{
  return card.reduced (juce::jmax (3, card.getWidth () / 40), 3);
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
  out.globalBounds = bounds.removeFromRight (bounds.getWidth () / 4);
  out.clipBounds = bounds;

  auto const paddingV = juce::jmax (4, out.clipBounds.getHeight () / 40);
  auto const headerH = juce::jmax (18, out.clipBounds.getHeight () / 12);
  out.headerHeight = headerH;

  auto area = out.clipBounds.reduced (paddingH, paddingV);

  auto headerArea = area.removeFromTop (headerH);
  out.readout = headerArea.removeFromRight (headerArea.getWidth () / 2);
  out.slotLabel = headerArea;

  area.removeFromTop (juce::jmax (4, out.clipBounds.getHeight () / 50));

  auto const gap = juce::jmax (2, out.clipBounds.getWidth () / 300);
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

    // Two by two, not four in a row: the section is a sixth of the bar wide
    // now, and four columns in it left each control a sliver. The two rows
    // are as tall as a control box rather than half the section, so the
    // knobs sit together instead of at opposite ends of the card.
    auto const gapH = juce::jmax (2, content.getWidth () / 20);
    auto const gapV = juce::jmax (2, content.getHeight () / 20);
    auto const motionRowH = juce::jmin (
        (content.getHeight () - gapV) / 2,
        controlBoxHeightForFont (bodySize, metrics.knobDiam));

    content.removeFromTop (
        (content.getHeight () - (2 * motionRowH + gapV)) / 2);

    auto topRow = content.removeFromTop (motionRowH);
    content.removeFromTop (gapV);
    auto bottomRow = content.removeFromTop (motionRowH);

    // The two values you dial on top, the two you pick from below: speed
    // and fade are continuous-ish, direction and end-action are lists.
    auto const colW = (topRow.getWidth () - gapH) / 2;
    auto const speedArea = topRow.removeFromLeft (colW);
    topRow.removeFromLeft (gapH);
    auto const seamArea = topRow;

    auto const directionArea = bottomRow.removeFromLeft (colW);
    bottomRow.removeFromLeft (gapH);
    auto const endActionArea = bottomRow;

    out.controls[2] = {
      textCell (speedArea, metrics.knobDiam),
      textCell (directionArea, metrics.knobDiam),
      textCell (endActionArea, metrics.knobDiam),
      textCell (seamArea, metrics.knobDiam),
    };

    // An open list takes over the section's controls, and only those: the
    // title stays put so the section is still named while you pick.
    out.motionDropdown = sectionContent (out.sectionCards[2])
                             .withTrimmedTop (out.sectionLabels[2].getHeight ());
  }

  // ── Global section ───────────────────────────────────────────────────
  {
    auto content = sectionContent (out.sectionCards[3]);
    out.sectionLabels[3]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    // The grid across the whole section, the four buttons in one row under
    // it. Beside each other the grid was cramped into two thirds of the
    // width while the strip beside it stood half empty.
    auto const buttonRowH
        = juce::jlimit (24, juce::jmax (24, content.getHeight () / 4),
                        metrics.knobDiam);
    auto const buttonGap = juce::jmax (2, buttonRowH / 8);
    auto buttons
        = content.removeFromBottom (2 * buttonRowH + buttonGap);
    content.removeFromBottom (juce::jmax (2, buttonRowH / 4));

    // ── the 4 x 3 grid ────────────────────────────────────────────────
    {
      auto const labelH = textRowHeight (content, metrics.captionSize);

      // As tall as a knob and its breathing room, not a third of whatever
      // is left: stretched to fill, the twelve knobs floated in cells
      // several times their size and the grid read as scattered dots.
      // A touch larger than the bar's standard knob: at knobDiam these
      // twelve read smaller than the ones in the clip's sections, because
      // they carry no caption of their own to give them presence.
      auto const gridKnob = static_cast<int> (metrics.knobDiam * 1.2f);

      auto const rowH = juce::jmin (
          (content.getHeight () - labelH) / numChannelRows,
          juce::jmax (labelH, juce::jmax (gridKnob + 2,
                                          static_cast<int> (
                                              gridKnob * 1.15f))));

      // Centred in what is left between the title and the buttons: capped
      // rows leave room over, and a block pinned to the top with a gap under
      // it looks like something fell off the bottom.
      auto const blockH = labelH + numChannelRows * rowH;
      content.removeFromTop ((content.getHeight () - blockH) / 2);

      auto grid = content.removeFromTop (blockH);
      auto headerRow = grid.removeFromTop (labelH);

      // Columns no wider than a knob needs: spread across the whole section
      // the four channels sat so far apart that reading a row meant
      // travelling the width of the bar.
      auto const gutterW = juce::jmax (labelH, grid.getWidth () / 12);
      auto const colW = juce::jmin (
          (grid.getWidth () - gutterW) / numChannelColumns,
          juce::jmax (labelH, juce::jmax (gridKnob + 2,
                                          static_cast<int> (
                                              gridKnob * 1.35f))));

      // Captions and knobs are centred together, as one block. Indenting
      // only the columns left "freq / Q / 3d" stranded at the far edge with
      // the knobs they name half a section away.
      auto const blockW = gutterW + colW * numChannelColumns;
      auto const indent = (grid.getWidth () - blockW) / 2;
      grid.removeFromLeft (indent);
      headerRow.removeFromLeft (indent);

      auto gutter = grid.removeFromLeft (gutterW);
      auto columns = grid;
      auto headerColumns = headerRow;
      headerColumns.removeFromLeft (gutterW);

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
                = column.removeFromTop (rowH).reduced (1);
        }
    }

    // ── two by two: rec mode, menu / rec, tap ─────────────────────────
    //
    // The rec mode is a button now too — it cycles the modes on a tap, which
    // is what its encoder used to do. Four in a row across the whole section
    // made each of them long and thin; two by two keeps them the shape of
    // something you press.
    {
      auto const gapH = juce::jmax (2, buttons.getWidth () / 60);
      auto const buttonW = (buttons.getWidth () - gapH) / 2;

      auto top = buttons.removeFromTop (buttonRowH);
      buttons.removeFromTop (buttonGap);
      auto bottom = buttons.removeFromTop (buttonRowH);

      out.recModeButton = top.removeFromLeft (buttonW);
      top.removeFromLeft (gapH);
      out.menuButton = top.removeFromLeft (buttonW);

      out.recButton = bottom.removeFromLeft (buttonW);
      bottom.removeFromLeft (gapH);
      out.tapButton = bottom.removeFromLeft (buttonW);

      // The rec mode no longer has a knob-style box of its own; its button
      // is where it lives. controls[3] stays so the encoder-era index does
      // not have to be special-cased away everywhere.
      out.controls[3] = { out.recModeButton };
    }
  }

  return out;
}

}
