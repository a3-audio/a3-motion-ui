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

#include <cmath>

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
      // clip-top, clip-bottom, then reach with the swell that sweeps it.
      // flat, flat-elevation and pole are gone: the base the graphic sets
      // says what they said, in one place you can see.
      return 4;
    case 2:
      // rot, spin, fade, bias, then dir and end along the floor -- what
      // shapes the movement in the plane.
      return 6;
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
    return subIndex == 4 || subIndex == 5;
  if (sectionIndex == 3)
    return subIndex == 0; // rec mode

  return false;
}

juce::Rectangle<int>
elevationCircleBounds (juce::Rectangle<int> cell)
{
  // The same 0.42 of the shorter side the graphic has always drawn at,
  // centred in the cell.
  auto const r = static_cast<int> (
      static_cast<float> (juce::jmin (cell.getWidth (), cell.getHeight ()))
      * 0.42f);

  return juce::Rectangle<int> (cell.getCentreX () - r, cell.getCentreY () - r,
                               r * 2, r * 2);
}

float
elevationBaseAt (juce::Rectangle<int> cell, int y, float bandLow,
                 float bandHigh)
{
  auto const circle = elevationCircleBounds (cell);
  if (circle.getHeight () <= 0)
    return 0.f;

  auto const frac = static_cast<float> (y - circle.getY ())
                    / static_cast<float> (circle.getHeight ());

  // Ordered before clamping, so clips pushed past each other pin the axis to
  // where they crossed rather than inverting the range.
  auto const low = juce::jmin (bandLow, bandHigh);
  auto const high = juce::jmax (bandLow, bandHigh);

  return juce::jlimit (low, high, juce::jlimit (0.f, 1.f, frac));
}

float
snapElevationBase (float base)
{
  // Wide enough to land on with a finger, narrow enough that a value just
  // above or below the ears can still be set.
  constexpr float earHeight = 0.5f;
  constexpr float pull = 0.02f;

  return std::abs (base - earHeight) <= pull ? earHeight : base;
}

bool
tapTogglesValue (int sectionIndex, int subIndex)
{
  // Nothing toggles any more: pole and flat were the last two, and the
  // elevation base replaced what they decided.
  juce::ignoreUnused (sectionIndex, subIndex);
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

  // Four channel faces lead the row, each with the toggle that says which of
  // that channel's two slots it means. They replaced CLIP and the two shared
  // slot keys: CLIP meant "show me the clip" and you had to remember whose,
  // while a face says whose and puts all four in front of you.
  auto const toggleW = juce::jmax (fingertipSize / 2, keyW * 2 / 3);
  auto const faceGap = juce::jmax (2, headerGap / 2);

  // What is left after the three words, the folder and the gaps between
  // everything, shared out over the four channels.
  auto const viewsW = juce::jmax (fingertipSize * 3,
                                  headerArea.getWidth () * 3 / 10);
  auto const roomForChannels
      = headerArea.getWidth () - viewsW - keyW
        - headerGap * (2 * static_cast<int> (numChannelColumns) + 5);
  auto const faceW = juce::jmax (
      fingertipSize,
      roomForChannels / static_cast<int> (numChannelColumns) - toggleW
          - faceGap);

  for (size_t channel = 0; channel < numChannelColumns; ++channel)
    {
      out.channelFaces[channel] = headerArea.removeFromLeft (faceW);
      headerArea.removeFromLeft (faceGap);
      out.channelSlotToggles[channel] = headerArea.removeFromLeft (toggleW);
      headerArea.removeFromLeft (headerGap);
    }

  // Set apart: "which clip" and "which view of it" are different questions.
  headerArea.removeFromLeft (headerGap * 2);

  // Three views now, not four: CLIP has no tab because the faces are it.
  auto const tabW = juce::jmax (
      fingertipSize,
      (headerArea.getWidth () - keyW - headerGap * 5) / 3);

  out.tabRecord = headerArea.removeFromLeft (tabW);
  headerArea.removeFromLeft (headerGap);
  // Between REC and PADS: ACTION is another way of looking at the clip, and
  // PADS is the view that is about something else.
  out.tabAction = headerArea.removeFromLeft (tabW);
  headerArea.removeFromLeft (headerGap);
  out.tabController = headerArea.removeFromLeft (tabW);

  // The folder closes the row. It is the way *out* of the clip you are on,
  // so it ends the row rather than leading it -- and it stands where the
  // slot keys used to, which is where a hand already goes for "something
  // else".
  headerArea.removeFromLeft (headerGap * 2);
  out.tabBrowser
      = headerArea.removeFromRight (juce::jmin (keyW, headerArea.getWidth ()));

  out.tabClip = {};
  for (index_t slot = 0; slot < numPadSlots; ++slot)
    out.slotButtons[slot] = {};

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
    auto const bandH = juce::jmin (
        content.getHeight (),
        buttonRows * out.buttonHeight + (buttonRows - 1) * gap);
    auto buttons = content.removeFromBottom (bandH);
    content.removeFromBottom (gap);

    // The rows share what the band has rather than each taking a full button
    // height from the top. Short of room the old way left the whole shortfall
    // on the last row, which then read as a mistake beside two full ones.
    auto const rowH
        = juce::jmax (1, (buttons.getHeight () - (buttonRows - 1) * gap)
                             / buttonRows);

    auto const perRow = 4;
    auto const colGap = juce::jmax (2, buttons.getWidth () / 60);
    auto const colW = (buttons.getWidth () - (perRow - 1) * colGap) / perRow;

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
          rows[static_cast<size_t> (r)] = buttons.removeFromTop (rowH);
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
    out.controls[0] = { out.trajectoryIcon };
  }

  // ── Elevation ────────────────────────────────────────────────────────
  {
    auto content = sectionContentBounds (out.sectionCards[1]);
    out.sectionLabels[1]
        = content.removeFromTop (titleRowHeight (content, headerSize));

    // The graphic takes a bigger share now: it is the control that sets where
    // the middle of the trajectory sits, so it has to be big enough to put a
    // finger on and read a line off.
    auto const gapV0 = juce::jmax (2, content.getHeight () / 20);
    out.elevationGraphic = content.removeFromTop (
        static_cast<int> (content.getHeight () * 0.5f));
    content.removeFromTop (gapV0);

    auto const gapV = juce::jmax (2, content.getHeight () / 30);
    auto const rowH = (content.getHeight () - gapV) / 2;
    auto row1 = content.removeFromTop (rowH);
    content.removeFromTop (gapV);
    auto row2 = content.removeFromTop (rowH);

    auto const gapH = juce::jmax (2, content.getWidth () / 20);
    auto const split = [gapH] (juce::Rectangle<int> &row) {
      auto const left = row.removeFromLeft (row.getWidth () / 2 - gapH / 2);
      row.removeFromLeft (gapH);
      return std::pair<juce::Rectangle<int>, juce::Rectangle<int> >{ left,
                                                                     row };
    };

    // The two clips above, then reach with the swell that sweeps it -- the
    // same pairing Motion uses, a standing value beside its movement.
    auto const [clipTopArea, clipBottomArea] = split (row1);
    auto const [reachArea, swellArea] = split (row2);

    out.controls[1] = {
      textCell (clipTopArea, metrics.knobDiam),
      textCell (clipBottomArea, metrics.knobDiam),
      textCell (reachArea, metrics.knobDiam),
      textCell (swellArea, metrics.knobDiam),
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
    // Two rows of two: rot with the spin that turns it, and the fade with the
    // bias that says where a drawn-through gap leads. reach and swell went
    // home to Elevation, where the sphere is.
    constexpr int motionKnobRows = 2;
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
    auto upperRow = knobRow (true);

    auto const colW = (lowerRow.getWidth () - gapH) / 2;
    auto const split = [colW, gapH] (juce::Rectangle<int> &row) {
      auto const left = row.removeFromLeft (colW);
      row.removeFromLeft (gapH);
      return std::pair<juce::Rectangle<int>, juce::Rectangle<int> >{ left,
                                                                     row };
    };

    auto const [upperLeft, upperRight] = split (upperRow);
    auto const [lowerLeft, lowerRight] = split (lowerRow);
    auto const [bottomLeft, bottomRight] = split (bottomRow);

    // The bottom row is already the button height; the cell is the button.
    auto const buttonCell = [] (juce::Rectangle<int> cell) { return cell; };

    // Each standing value with the movement that works on it, left and right.
    // The two lists close the section along its floor, where every other
    // section's buttons are.
    out.controls[2] = {
      textCell (upperLeft, metrics.knobDiam),  // rot
      textCell (upperRight, metrics.knobDiam), // spin
      textCell (lowerLeft, metrics.knobDiam),  // fade
      textCell (lowerRight, metrics.knobDiam), // bias
      buttonCell (bottomLeft),                 // direction
      buttonCell (bottomRight),                // end action
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

  return out;
}

}
