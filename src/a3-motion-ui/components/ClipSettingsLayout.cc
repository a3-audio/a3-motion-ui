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

#include <array>
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
      return 1; // just the shape now -- rot, fade and bias went to Motion
    case 1:
      return 6; // reach, clip-top, clip-bottom, mirror-south, flat, flat-elev
    case 2:
      // spin, swell, rot, fade, bias, then dir and end along the floor.
      // Everything that shapes a movement over time lives here.
      return 7;
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
    // direction and end action, along Motion's floor. They used to open a
    // list; a list covered the controls under it, and both are short enough
    // that a finger can simply walk them.
    return subIndex == 5 || subIndex == 6;
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
driftMark (juce::Rectangle<int> bounds)
{
  if (bounds.isEmpty ())
    return {};

  auto const size = juce::jmax (
      3, juce::jmin (bounds.getWidth (), bounds.getHeight ()) / 5);
  auto const inset = juce::jmax (2, size / 2);

  return juce::Rectangle<int> (size, size)
      .withPosition (bounds.getRight () - size - inset,
                     bounds.getY () + inset);
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
                    float bodySize, float potSizeScale, BarPage page)
{
  ClipSettingsLayout out;

  // Two panels side by side, not one panel with an odd section on the end.
  out.globalBounds = bounds.removeFromRight (bounds.getWidth () / 4);
  out.clipBounds = bounds;

  auto const paddingV = juce::jmax (4, out.clipBounds.getHeight () / 40);
  // Tall enough to hit. Every control in this row is pressed mid-set by a hand
  // that is also doing something else, and a twelfth of the bar left them
  // under a fingertip -- the same floor the pads and tabs already keep.
  auto const headerH
      = juce::jmin (juce::jmax (fingertipSize, out.clipBounds.getHeight () / 9),
                    juce::jmax (18, out.clipBounds.getHeight () / 6));
  out.headerHeight = headerH;

  auto area = out.clipBounds.reduced (paddingH, paddingV);

  auto headerArea = area.removeFromTop (headerH);

  // Left to right, in the order they are reached for: the folder, the three
  // views of the clip, the two slots, then the four things you do to it. The
  // browser leads because it is where a set begins; the transport closes
  // because it is what you touch while everything else is already decided.
  auto const headerGap = juce::jmax (2, headerH / 12);

  // Square keys for everything that is a mark rather than a word: the folder,
  // the two slots, the four transport marks. A slot is "1" and "2" now -- the
  // word "Slot" was three quarters of a key spent saying what the two keys
  // being side by side already says.
  auto const keyW = headerH;
  auto const takeKey = [&headerArea, keyW, headerGap] () {
    auto const key = headerArea.removeFromLeft (keyW);
    headerArea.removeFromLeft (headerGap);
    return key;
  };

  out.tabBrowser = takeKey ();
  headerArea.removeFromLeft (headerGap * 3);

  // The three views keep words, and words need room. What is left after the
  // marks, shared between them.
  // Counted, not estimated: six square keys, and fourteen gaps -- two between
  // the tabs, two separators of three, and one after each key. Two short of
  // that and the last transport key comes out narrower than the rest, which
  // is how a row of equal marks stops looking like a row of equal marks.
  auto const keysAfter = static_cast<int> (numPadSlots);
  auto const gapsAfter = 2 + 3 + static_cast<int> (numPadSlots);
  auto const marksAfter = keyW * keysAfter + headerGap * gapsAfter;

  // Four views now, not three -- and they fit because the four transport keys
  // left this row for the band over the global strip. That is the whole trade:
  // what the transport gave up, ACTION took.
  auto const tabW = juce::jmax (
      fingertipSize, (headerArea.getWidth () - marksAfter) / 4);

  out.tabClip = headerArea.removeFromLeft (tabW);
  headerArea.removeFromLeft (headerGap);
  out.tabRecord = headerArea.removeFromLeft (tabW);
  headerArea.removeFromLeft (headerGap);
  // Between REC and PADS: ACTION is another way of looking at the clip, and
  // PADS is the view that is about something else.
  out.tabAction = headerArea.removeFromLeft (tabW);
  headerArea.removeFromLeft (headerGap);
  out.tabController = headerArea.removeFromLeft (tabW);

  // Set apart from the tabs: "which clip" and "which view of it" are
  // different questions.
  headerArea.removeFromLeft (headerGap * 3);

  if (page != BarPage::Controller && page != BarPage::Browser)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      out.slotButtons[slot] = takeKey ();
  else
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      headerArea.removeFromLeft (keyW + headerGap);

  // The transport has left this row. It stands over the global strip now --
  // rec, stop, play and act belong to the device the way MENU and TAP do, and
  // taking them out of here is what leaves room for a fourth view of the clip.

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
  // Half again over a knob was too tall once the Shape section had a picture
  // worth looking at -- every row the buttons took came off it. The 34px floor
  // stays: below it TAP could not be hit reliably, and that finding is about
  // fingers, not about how much room the picture would like.
  out.buttonHeight = juce::jlimit (
      34, juce::jmax (34, out.clipBounds.getHeight () / 6),
      static_cast<int> (metrics.knobDiam * 1.35f));

  // Shape, then Motion, then Elevation. What a clip is and how it moves are
  // what a hand reaches for while playing; where it sits on the sphere is set
  // once and left alone, so it goes to the far end. The indices stay as they
  // were -- only the places change, which keeps every sub-index list intact.
  constexpr int sectionOrder[] = { 0, 2, 1 };
  for (auto const index : sectionOrder)
    out.sectionCards[static_cast<size_t> (index)]
        = area.removeFromLeft (sectionW).reduced (gap / 2, 0);

  auto globalArea = out.globalBounds.reduced (paddingH, paddingV);

  // The readout goes in the band above the strip's card — the same band the
  // slot label and the tabs are on, so the bar reads across at one height.
  // Over the *global* strip because what it reports comes from either page,
  // and dropped into the card instead it would take a row the channel grid
  // needs: at the smallest skin sizes that collapsed its cells to five pixels.
  // The band the readout used to stand in. The readout is in the status bar
  // now -- a reading among readings -- and the four things you do to a clip
  // stand here instead, over the strip that also carries MENU, REC and TAP.
  // The card starts here, above the transport rather than under it: the four
  // things you do to a clip and the six device keys are one block, and a row
  // of keys floating over the panel they belong to read as an afterthought.
  auto const globalCard = globalArea;
  auto transportBand = globalArea.removeFromTop (headerH);
  out.readout = {};

  // Not on the pads page, which is these four controls already.
  if (page != BarPage::Controller)
    {
      // Square, like every other mark on the bar -- a finger knows what a
      // square key is. Held to what the band can carry, and the row centred
      // in it so a narrow strip loses room evenly at both ends.
      auto const transportGap = juce::jmax (2, headerH / 12);
      auto const keySide = juce::jmin (
          transportBand.getHeight (),
          (transportBand.getWidth () - (numTransportKeys - 1) * transportGap)
              / numTransportKeys);
      auto const rowW
          = keySide * numTransportKeys + transportGap * (numTransportKeys - 1);

      auto row = transportBand.withSizeKeepingCentre (
          rowW, juce::jmin (transportBand.getHeight (), keySide));

      for (int i = 0; i < numTransportKeys; ++i)
        {
          out.transportButtons[static_cast<size_t> (i)]
              = row.removeFromLeft (keySide);
          if (i + 1 < numTransportKeys)
            row.removeFromLeft (transportGap);
        }
    }

  out.sectionCards[3] = globalCard.reduced (gap / 2, 0);
  // What the strip lays out in: the card less the band the transport took.
  out.globalContent
      = sectionContentBounds (out.sectionCards[3]).withTrimmedTop (headerH);

  // ── Shape, and its other side ────────────────────────────────────────
  //
  // One card, two faces. The front is the clip as it plays — what shape, how
  // fast, which way round. The back, which REC turns to, is the take you are
  // about to make: how long, how its join is closed, and the trajectory
  // appearing as you play it in. The card does not move between them: it is
  // one section showing one side or the other.
  {
    auto const recording = page == BarPage::Record;

    auto content = sectionContentBounds (out.sectionCards[0]);
    out.sectionLabels[0]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    auto const gap = juce::jmax (2, out.buttonHeight / 8);

    // Twelve speeds want three rows, eight lengths two. The buttons are the
    // section's floor either way, so the bar still reads as one row of
    // buttons across its bottom.
    auto const buttonRows = recording ? 2 : 3;
    auto buttons = content.removeFromBottom (
        buttonRows * out.buttonHeight + (buttonRows - 1) * gap);
    content.removeFromBottom (gap);

    // The picture and the knob stand on the button grid rather than beside it:
    // three columns for the trajectory, one for the knob, on the same column
    // width the buttons use. A row that nearly lines up with the grid under it
    // reads as a mistake; one that lines up exactly reads as structure.
    auto const perRow = 4;
    auto const colGap = juce::jmax (2, buttons.getWidth () / 60);
    auto const colW = (buttons.getWidth () - (perRow - 1) * colGap) / perRow;

    // Measured from the left, exactly as place() steps the buttons across.
    // Taken from the right instead it was three pixels out: colW is an integer
    // division, so the remainder sits against the right edge and everything
    // referenced to that edge is off by it.
    auto const knobColumn
        = content.withX (content.getX () + 3 * (colW + colGap))
              .withWidth (colW);
    content = content.withWidth (3 * colW + 2 * colGap);

    // The name lies over the picture rather than under it. As a caption it
    // cost the picture a whole row and told you something you mostly already
    // know -- you chose the trajectory. Over it, it is there when you look for
    // it and out of the way when you are reading the shape.
    out.trajectoryIcon = content;
    out.trajectoryName = content;

    {
      std::array<juce::Rectangle<int>, 3> rows;
      for (int r = 0; r < buttonRows; ++r)
        {
          rows[static_cast<size_t> (r)]
              = buttons.removeFromTop (out.buttonHeight);
          if (r + 1 < buttonRows)
            buttons.removeFromTop (gap);
        }

      auto const place = [&] (int index, juce::Rectangle<int> &into) {
        auto &row = rows[static_cast<size_t> (index / perRow)];
        into = row.removeFromLeft (colW);
        row.removeFromLeft (colGap);
      };

      if (recording)
        for (int i = 0; i < numRecordLengths; ++i)
          place (i, out.lengthButtons[static_cast<size_t> (i)]);
      else
        for (int i = 0; i < numSpeedButtons; ++i)
          place (i, out.speedButtons[static_cast<size_t> (i)]);
    }

    // The picture, and nothing else. The lengths and the speeds are buttons of
    // their own -- they are not values a finger turns, so they are not
    // sub-elements of the section.
    //
    // The knob column went with rot, fade and bias to Motion, where the things
    // that shape a movement over time belong. What is left is the picture and
    // the buttons under it.
    juce::ignoreUnused (knobColumn);

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

    // Two rows of two. Three knob rows and a button row was the old shape,
    // back when eight things lived here; the envelope and the act mode have
    // gone to the ACTION page and what is left breathes.
    //
    // Shared out rather than taken one after another from the bottom. A skin
    // can cut the bar down (clipSettingsHeightScale), and a section that helps
    // itself row by row leaves the whole shortfall on the row at the top.
    // Three now, not two: rot, fade and bias arrived from Shape and joined
    // spin and swell. Five knobs over three rows of two, with the last cell
    // left empty rather than squeezing everything into two rows -- a knob
    // under a fingertip is worth more than a tidy grid.
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

    auto lowerRow = knobRow (false);
    auto middleRow = knobRow (false);
    auto upperRow = knobRow (true);

    auto const colW = (lowerRow.getWidth () - gapH) / 2;
    auto const split = [colW, gapH] (juce::Rectangle<int> &row) {
      auto const left = row.removeFromLeft (colW);
      row.removeFromLeft (gapH);
      return std::pair<juce::Rectangle<int>, juce::Rectangle<int> >{ left,
                                                                     row };
    };

    auto const [upperLeft, upperRight] = split (upperRow);
    auto const [middleLeft, middleRight] = split (middleRow);
    auto const [lowerLeft, lowerRight] = split (lowerRow);
    auto const [bottomLeft, bottomRight] = split (bottomRow);

    // The bottom row is already the button height; the cell is the button.
    auto const buttonCell = [] (juce::Rectangle<int> cell) { return cell; };

    // Everything that shapes a movement over time. spin turns the shape under
    // the blob and swell opens and closes how far down the sphere it reaches;
    // rot is the standing angle the spin adds to, and fade and bias say which
    // of the take's gaps are drawn through and where they lead. The two lists
    // close the section along its floor, where every other section's buttons
    // are.
    juce::ignoreUnused (lowerRight);
    out.controls[2] = {
      textCell (upperLeft, metrics.knobDiam),   // spin
      textCell (upperRight, metrics.knobDiam),  // swell
      textCell (middleLeft, metrics.knobDiam),  // rot
      textCell (middleRight, metrics.knobDiam), // fade
      textCell (lowerLeft, metrics.knobDiam),   // bias
      buttonCell (bottomLeft),                  // direction
      buttonCell (bottomRight),                 // end action
    };

  }

  // ── Global section ───────────────────────────────────────────────────
  {
    // No title: "global" named a panel whose contents name themselves -- the
    // rows are written beside the knobs and the keys carry words -- and the
    // row it took is a row the twelve knobs wanted.
    auto content = out.globalContent;
    out.sectionLabels[3] = {};

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
  for (int i = 0; i < numClipSettingsSections - 1; ++i)
    {
      auto const c = static_cast<size_t> (i);
      out.dropdownArea[c] = sectionContentBounds (out.sectionCards[c])
                                .withTrimmedTop (
                                    out.sectionLabels[c].getHeight ());
    }

  // The strip is measured differently -- its card reaches up over the
  // transport band, which nothing may open across.
  out.dropdownArea[3] = out.globalContent;

  return out;
}

}
