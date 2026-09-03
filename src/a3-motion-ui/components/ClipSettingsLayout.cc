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

#include <tuple>

#include "ControllerLayout.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>

namespace a3
{

namespace
{
// The bar's own margin. The screen edge is already an edge; 16 here put a
// finger's width of nothing between it and the first section.
constexpr int paddingH = 8;

// The global section takes a quarter of the bar and the clip's three
// sections share the rest. It had half while its grid was spread across the
// whole width; capped to what the knobs need, the grid fits in a quarter and
// the clip's sections get the room back.
int
clipSectionWidth (int rowWidth)
{
  return rowWidth * 3 / 4 / (numClipSettingsSections - 1);
}

}

juce::Rectangle<int>
sectionContentBounds (juce::Rectangle<int> card)
{
  // A hairline, not a border zone. It was a fortieth of the card wide, which
  // at the device's sixth-of-the-bar sections took width from rows that hold
  // four values.
  return card.reduced (juce::jmax (2, card.getWidth () / 80), 3);
}

int
numControlsInSection (int sectionIndex)
{
  switch (sectionIndex)
    {
    case 0:
      return 1; // the shape in the slot; the lengths are buttons of their own
    case 1:
      return 6; // reach, clip-top, clip-bottom, mirror-south, flat, flat-elev
    case 2:
      return 8; // speed, direction, end-action, seam, spin, swell, atk, dec
    case 3:
      return 1; // rec mode — the global section's only encoder-ish value
    default:
      return 0;
    }
}

bool
tapAdvancesValue (int sectionIndex, int subIndex)
{
  if (sectionIndex == 2)
    return subIndex == 1 || subIndex == 2; // direction, end-action
  if (sectionIndex == 3)
    return subIndex == 0; // rec mode

  return false;
}

bool
tapTogglesValue (int sectionIndex, int subIndex)
{
  if (sectionIndex == 1)
    return subIndex == 3 || subIndex == 4; // pole, flat

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
  // The two pages close the row, where the readout used to sit. Sized to be
  // hit rather than to fit their words — they are switched mid-set, with one
  // hand, without looking down.
  auto const tabW = juce::jmax (fingertipSize, headerArea.getWidth () / 4);
  out.tabController = headerArea.removeFromRight (tabW);
  out.tabClip = headerArea.removeFromRight (tabW);
  out.slotLabel = headerArea;

  area.removeFromTop (juce::jmax (4, out.clipBounds.getHeight () / 50));

  out.clipContent = area;

  auto const gap = juce::jmax (2, out.clipBounds.getWidth () / 300);
  auto const sectionW = area.getWidth () / (numClipSettingsSections - 1);

  auto const knobDiam = knobDiameterForFont (bodySize, potSizeScale);
  auto const cardW = sectionW - 2 * (gap / 2);
  // The width the controls actually get, not a second guess at it. These
  // were separate once and the fonts were fitted to the narrower of the two.
  auto const sectionContentW
      = sectionContentBounds ({ 0, 0, cardW, 1 }).getWidth ();
  auto const columnGap = juce::jmax (2, sectionContentW / 20);
  auto const controlBoxH = controlBoxHeightForFont (bodySize, knobDiam);

  out.metrics = ControlMetrics{
    knobDiam,
    sharedCaptionSize (bodySize, sectionContentW, columnGap, controlBoxH),
    sharedValueSize (bodySize, sectionContentW, columnGap, controlBoxH)
  };

  auto const &metrics = out.metrics;

  // One height for every button in the bar. Worked out before any section is
  // laid out, so Elevation's, Motion's and the global ones cannot drift
  // apart.
  // Half again over a knob, and never under 34px. At knobDiam the buttons
  // came out 24 high at the shipped sizes, which is under a fingertip — the
  // maintainer could not hit TAP reliably.
  out.buttonHeight = juce::jlimit (
      34, juce::jmax (34, out.clipBounds.getHeight () / 5),
      static_cast<int> (metrics.knobDiam * 1.6f));

  for (int i = 0; i < numClipSettingsSections - 1; ++i)
    out.sectionCards[static_cast<size_t> (i)]
        = area.removeFromLeft (sectionW).reduced (gap / 2, 0);

  auto globalArea = out.globalBounds.reduced (paddingH, paddingV);

  // The readout goes in the band above the strip's card — the same band the
  // slot label and the tabs are on, so the bar reads across at one height.
  // Over the *global* strip because what it reports comes from either page,
  // and dropped into the card instead it would take a row the channel grid
  // needs: at the smallest skin sizes that collapsed its cells to five pixels.
  out.readout = globalArea.removeFromTop (headerH);
  out.sectionCards[3] = globalArea.reduced (gap / 2, 0);

  // ── Shape ────────────────────────────────────────────────────────────
  {
    auto content = sectionContentBounds (out.sectionCards[0]);
    out.sectionLabels[0]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    // Two rows of four length buttons on the section's floor.
    auto const gap = juce::jmax (2, out.buttonHeight / 8);
    auto buttons = content.removeFromBottom (2 * out.buttonHeight + gap);
    content.removeFromBottom (gap);

    // The name sits *in* the pictogram, centred, rather than on a strip
    // beneath it. On its own row it ended up pressed against the button
    // below, reading as that button's second caption.
    out.trajectoryIcon = content;
    out.trajectoryName = content;

    {
      auto const perRow = 4;
      auto const colGap = juce::jmax (2, buttons.getWidth () / 60);
      auto const colW = (buttons.getWidth () - (perRow - 1) * colGap) / perRow;

      auto top = buttons.removeFromTop (out.buttonHeight);
      buttons.removeFromTop (gap);
      auto bottom = buttons;

      for (int i = 0; i < numRecordLengths; ++i)
        {
          auto &row = i < perRow ? top : bottom;
          out.lengthButtons[static_cast<size_t> (i)]
              = row.removeFromLeft (colW);
          row.removeFromLeft (colGap);
        }
    }

    // Only the pictogram is a control of the clip's; the lengths are a
    // setting for the next take and have their own buttons.
    out.controls[0] = { out.trajectoryIcon };
  }

  // ── Elevation ────────────────────────────────────────────────────────
  {
    auto content = sectionContentBounds (out.sectionCards[1]);
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

    // Top row the two clips, middle row flat-elevation beside reach, bottom
    // row the two buttons. Reading down the right: reach, then pole; down
    // the left: clip-top, flat-elevation, flat — each value above the button
    // that switches it off.
    auto const clipTopArea
        = row1.removeFromLeft (row1.getWidth () / 2 - gapH / 2);
    row1.removeFromLeft (gapH);
    auto const clipBottomArea = row1;

    auto const flatElevationArea
        = row2.removeFromLeft (row2.getWidth () / 2 - gapH / 2);
    row2.removeFromLeft (gapH);
    auto const reachArea = row2;

    auto const flatArea = row3.removeFromLeft (row3.getWidth () / 2 - gapH / 2);
    row3.removeFromLeft (gapH);
    auto const mirrorArea = row3;

    // By sub-index, not by row: mirror-south is 3 but sits on the first row.
    // The two buttons take the bar's shared button height and sit at the
    // bottom of their cell — they are the floor of the section, and centred
    // they floated above it. The caption lives inside the button now, so no
    // row is reserved under it.
    auto const buttonCell = [&] (juce::Rectangle<int> cell) {
      auto const h = juce::jmin (cell.getHeight (), out.buttonHeight);
      return juce::Rectangle<int> (cell.getX (), cell.getBottom () - h,
                                   cell.getWidth (), h);
    };

    out.controls[1] = {
      textCell (reachArea, metrics.knobDiam),
      textCell (clipTopArea, metrics.knobDiam),
      textCell (clipBottomArea, metrics.knobDiam),
      buttonCell (mirrorArea),
      buttonCell (flatArea),
      textCell (flatElevationArea, metrics.knobDiam),
    };
  }

  // ── Motion ───────────────────────────────────────────────────────────
  {
    auto content = sectionContentBounds (out.sectionCards[2]);
    out.sectionLabels[2]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    // Two by two, not four in a row: the section is a sixth of the bar wide
    // now, and four columns in it left each control a sliver.
    //
    // The two buttons go to the section's floor, where Elevation's are —
    // the bar reads as one row of buttons across its bottom rather than
    // three sections each arranging their own. The knobs then sit centred
    // in what is left above them.
    auto const gapH = juce::jmax (2, content.getWidth () / 20);
    auto const gapV = juce::jmax (2, content.getHeight () / 20);

    auto bottomRow = content.removeFromBottom (
        juce::jmin (content.getHeight (), out.buttonHeight));
    content.removeFromBottom (gapV);

    // Three rows of what moves on its own, stacked above the pair that does
    // not. spin turns the shape under the blob and swell opens and closes how
    // far down the sphere it reaches — same table, same bipolar knob, same
    // standstill in the middle. Above them the accent's two, which are a
    // gesture rather than a cycle and so have a table of their own.
    //
    // Shared out rather than taken one after another from the bottom. A skin
    // can cut the bar down (clipSettingsHeightScale), and a section that helps
    // itself row by row leaves the whole shortfall on the row at the top —
    // which came out a sliver while the three below it were untouched. Short
    // of room, all three give up the same.
    constexpr int motionKnobRows = 3;
    auto const wanted = controlBoxHeightForFont (bodySize, metrics.knobDiam);
    auto const available
        = (content.getHeight () - (motionKnobRows - 1) * gapV) / motionKnobRows;
    auto const motionRowH = juce::jmax (1, juce::jmin (wanted, available));

    auto const knobRow = [&content, motionRowH, gapV] (bool last) {
      auto row = content.removeFromBottom (motionRowH);
      if (!last)
        content.removeFromBottom (gapV);
      return row;
    };

    auto topRow = knobRow (false);
    auto lfoRow = knobRow (false);
    auto envRow = knobRow (true);

    // The two values you dial on top, the two you pick from below: speed
    // and fade are continuous-ish, direction and end-action are lists.
    auto const colW = (topRow.getWidth () - gapH) / 2;
    auto const spinArea = lfoRow.removeFromLeft (colW);
    lfoRow.removeFromLeft (gapH);
    auto const swellArea = lfoRow;

    auto const attackArea = envRow.removeFromLeft (colW);
    envRow.removeFromLeft (gapH);
    auto const decayArea = envRow;

    auto const speedArea = topRow.removeFromLeft (colW);
    topRow.removeFromLeft (gapH);
    auto const seamArea = topRow;

    auto const directionArea = bottomRow.removeFromLeft (colW);
    bottomRow.removeFromLeft (gapH);
    auto const endActionArea = bottomRow;

    // The bottom row is already the button height; the cell is the button.
    auto const buttonCell = [] (juce::Rectangle<int> cell) { return cell; };

    // Appended, not inserted: the four before it are what a finger has
    // already learned to find.
    out.controls[2] = {
      textCell (speedArea, metrics.knobDiam),
      buttonCell (directionArea),
      buttonCell (endActionArea),
      textCell (seamArea, metrics.knobDiam),
      textCell (spinArea, metrics.knobDiam),
      textCell (swellArea, metrics.knobDiam),
      textCell (attackArea, metrics.knobDiam),
      textCell (decayArea, metrics.knobDiam),
    };
  }

  // ── Global section ───────────────────────────────────────────────────
  {
    auto content = sectionContentBounds (out.sectionCards[3]);

    out.sectionLabels[3]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    // The grid across the whole section, the four buttons in one row under
    // it. Beside each other the grid was cramped into two thirds of the
    // width while the strip beside it stood half empty.
    // The bar's one button height — the same one Elevation's and Motion's
    // buttons get.
    auto const buttonRowH = out.buttonHeight;
    auto const buttonGap = juce::jmax (2, buttonRowH / 8);
    auto buttons
        = content.removeFromBottom (3 * buttonRowH + 2 * buttonGap);
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

      // Straight under the section's title. Centred, the block drifted down
      // as the bar grew and left the channel numbers a long way from the
      // heading that names the section they belong to.
      auto const blockH = labelH + numChannelRows * rowH;
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

    // ── the six function keys, two by three ───────────────────────────
    //
    // These stand for the panel's six function keys, so they are laid out
    // like them: one size for all of them, filled top-left to bottom-right.
    // Left down: tap, clock, rec. Right down: recmode, menu, shift. A button
    // sized differently from its neighbours reads as a different kind of
    // thing, and all six are the same kind — the thing your hand goes to
    // without looking.
    {
      auto const gapH = juce::jmax (2, buttons.getWidth () / 60);
      auto const buttonW = (buttons.getWidth () - gapH) / 2;

      auto row = [&] (bool last) {
        auto r = buttons.removeFromTop (buttonRowH);
        if (!last)
          buttons.removeFromTop (buttonGap);

        auto const left = r.removeFromLeft (buttonW);
        r.removeFromLeft (gapH);
        return std::pair<juce::Rectangle<int>, juce::Rectangle<int> >{
          left, r.removeFromLeft (buttonW)
        };
      };

      std::tie (out.tapButton, out.recModeButton) = row (false);
      std::tie (out.clockModeButton, out.menuButton) = row (false);
      std::tie (out.recButton, out.shiftButton) = row (true);

      // The rec mode no longer has a knob-style box of its own; its button
      // is where it lives. controls[3] stays so the encoder-era index does
      // not have to be special-cased away everywhere.
      out.controls[3] = { out.recModeButton };
    }
  }

  // An open list takes over a section's controls, and only those: the title
  // stays put so the section is still named while you pick.
  for (int i = 0; i < numClipSettingsSections; ++i)
    {
      auto const c = static_cast<size_t> (i);
      out.dropdownArea[c] = sectionContentBounds (out.sectionCards[c])
                                .withTrimmedTop (
                                    out.sectionLabels[c].getHeight ());
    }

  return out;
}

}
