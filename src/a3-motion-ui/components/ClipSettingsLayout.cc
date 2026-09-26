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

#include <algorithm>

#include <cmath>

#include <array>
#include <tuple>

#include "ControllerLayout.hh"

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-engine/PlaybackRate.hh>
#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

juce::String
beatsName (float beats)
{
  if (!(beats > 0.f))
    return "--";

  // Halved until the numerator is whole, which it always becomes: every length
  // here is a take's beat count times a power of two. Bounded anyway -- a loop
  // that trusts floating point to land exactly is a loop that one day does
  // not.
  auto numerator = static_cast<double> (beats);
  auto denominator = 1;
  for (int i = 0; i < 12 && std::abs (numerator - std::round (numerator)) > 1e-4;
       ++i)
    {
      numerator *= 2.0;
      denominator *= 2;
    }

  auto whole = static_cast<long> (std::lround (numerator));
  if (whole < 1)
    whole = 1;

  // In lowest terms, so four eighths is a half and eight eighths is one.
  auto const divisor = std::gcd (whole, static_cast<long> (denominator));
  whole /= divisor;
  denominator /= static_cast<int> (divisor);

  return denominator == 1 ? juce::String (whole)
                          : juce::String (whole) + "/"
                                + juce::String (denominator);
}

juce::String
speedKeyName (int speedLog2, float patternLengthBeats)
{
  // Through the engine's own function, not a second copy of the arithmetic.
  // The whole point of the key saying a number of ticks is that the number is
  // the one the engine plays; two expressions that happen to agree today are
  // a label and a playback length waiting to drift apart.
  return beatsName (playbackLengthBeats (patternLengthBeats, speedLog2));
}

int
selectionAfterRemoving (int row, int remaining)
{
  if (remaining <= 0)
    return 0;

  return std::clamp (row, 0, remaining - 1);
}


int
draggedSpeedLog2 (int speedLog2, int increment)
{
  return std::clamp (speedLog2 + increment, speedLog2Min, speedLog2Max);
}

bool
speedKeyIsActive (std::array<int, numSpeedButtons> const &keys, int index,
                  int clipSpeedLog2, int draggedIndex)
{
  if (index < 0 || index >= numSpeedButtons)
    return false;

  // A key under a finger is the only one that has anything to say. The value
  // is on its way somewhere and walks through what the other keys carry to
  // get there; lighting them as it passes is four keys taking turns, which is
  // the picture this gesture was changed to be rid of. On release the rule
  // below resumes on the same key, because by then it carries the speed.
  if (draggedIndex != noSpeedKeyDragged)
    return index == draggedIndex;

  // Two keys given the same speed both light. That is the truth about them,
  // and a display that picked one would disagree with the keys themselves.
  return keys[static_cast<size_t> (index)] == clipSpeedLog2;
}

namespace
{
// The bar's own margin. The screen edge is already an edge; this puts a
// finger's width of nothing between it and the first section -- the skin's
// `padding`, not a fixed number, since it became skinnable.
//
// A function, not a constant: a namespace-scope constant would read the
// theme during static initialisation -- before the first setTheme -- and
// would never follow a skin change afterwards.
int
paddingH ()
{
  return juce::roundToInt (theme ().padding);
}

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
  return card.reduced (juce::jmax (2, card.getWidth () / 80),
                       juce::roundToInt (theme ().paddingTight));
}

int
numControlsInSection (int sectionIndex)
{
  switch (sectionIndex)
    {
    case 0:
      // The picture, the clip field, then the direction and the end action.
      // Those two came from Motion: what a pass does when it runs out is a
      // property of the take, and the take is what this section is about --
      // Motion is what the movement *is*, not how it is played through.
      return 4;
    case 1:
      // clip-top, clip-bottom, the sway, then elv. reach went to Motion to
      // stand beside the swell that sweeps it, and the sway came here in its
      // place, because where the middle of the trajectory sits is what the
      // graphic above draws. elv is that middle: set by a finger on the
      // graphic until 2026-09-26, a knob since.
      return 4;
    case 2:
      // Ten knobs in five rows, and no buttons: the two lists went to Shape.
      // Numbered in reading order for the first time -- rot, spin, reach,
      // swell, sqzX, strX, sqzY, strY, fade, bias -- because everything in
      // the section had to move anyway when the lists left, and an order that
      // is the order things are read in is one nobody has to look up.
      return 10;
    case 3:
      return 1; // rec mode — the global section's only encoder-ish value
    default:
      return 0;
    }
}

bool
tapAdvancesValue (int sectionIndex, int subIndex)
{
  if (sectionIndex == 0)
    // direction and end action, under Shape's speeds. They step on a tap:
    // both are short enough that a finger can walk them, and a list would
    // cover the picture they belong to. They came from Motion with the rest
    // of what a take does when it runs out.
    return subIndex == 2 || subIndex == 3;
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
elevationBaseForKnob (float knob, float clipTop, float clipBottom)
{
  // What clip-top and clip-bottom have left of the sphere, ordered before the
  // clamp so clips pushed past each other pin the axis to where they crossed
  // rather than inverting the range. Snapped first and clamped last, so the
  // snap cannot pull the line out of the band.
  auto const top = std::clamp (clipTop, 0.f, 1.f);
  auto const bottom = 1.f - std::clamp (clipBottom, 0.f, 1.f);
  auto const low = juce::jmin (top, bottom);
  auto const high = juce::jmax (top, bottom);

  auto const base = snapElevationBase (knobForElevationBase (
      std::clamp (knob, 0.f, 1.f)));
  return juce::jlimit (low, high, base);
}

float
snapElevationBase (float base)
{
  // Wide enough to land on with a finger, narrow enough that a value just
  // above or below one of them can still be set.
  constexpr float pull = 0.02f;

  // Ear height, and the two poles.
  //
  // The poles matter for a reason the ears do not: a figure whose middle
  // crosses the middle of the pad is torn there at every base but a pole, and
  // "but a pole" means exactly nought or exactly one. A finger cannot land on
  // exactly a float, so without this the one setting that closes the hole was
  // the one setting a hand could not ask for.
  for (auto const at : { 0.f, 0.5f, 1.f })
    if (std::abs (base - at) <= pull)
      return at;

  return base;
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

juce::Rectangle<int>
cardOfControl (ClipSettingsLayout const &layout, int section, int sub)
{
  // dir and end stand in CLIP's middle card; fade, bias and the rec mode in
  // REC's; everything else in its section's own.
  if (section == 0 && sub >= 2)
    return layout.playCard;
  if (section == 3 || (section == 2 && controlIsOnPage (2, sub, BarPage::Record)))
    return layout.recordCard;

  return layout.sectionCards[static_cast<size_t> (section)];
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

  // This is the panel's own margin -- the same kind of thing as paddingH()
  // above it -- which is why it took a role and the many jmax (2, ...) /
  // jmax (4, ...) gaps between things further down this function did not:
  // there is no role for a gap, only for a margin around the outside of the
  // whole panel. A skinned paddingSmall widens this margin and leaves every
  // inner gap exactly as it was.
  auto const paddingV
      = juce::jmax (juce::roundToInt (theme ().paddingSmall),
                    barPadding.of (out.clipBounds.getHeight ()));
  // Tall enough to hit. Every control in this row is pressed mid-set by a hand
  // that is also doing something else, and a twelfth of the bar left them
  // under a fingertip -- the same floor the pads and tabs already keep.
  auto const headerH = juce::jmin (
      juce::jmax (fingertipSize, barHeader.of (out.clipBounds.getHeight ())),
      juce::jmax (18, barHeaderMax.of (out.clipBounds.getHeight ())));
  out.headerHeight = headerH;

  auto area = out.clipBounds.reduced (paddingH (), paddingV);

  auto headerArea = area.removeFromTop (headerH);

  // Left to right, as the maintainer set it on 2026-09-26: CLIP MOTION ACTION
  // FILES CHMIX MAINMIX REC PADS. The channel faces that led the row stand at the top of
  // the global strip since then -- see there.
  auto const headerGap = juce::jmax (2, headerGapOfHeader.of (headerH));

  constexpr int numViews = 8;
  constexpr int numHeaderGaps = numViews - 1;

  // **Every edge is computed from the row's whole width, not stepped across
  // it.** Stepping meant an integer span times five, and the remainder of that
  // division piled up against the right edge -- which is where the eye reads
  // the row as finished or not. Cumulative, the last edge lands on the right
  // edge by construction and the remainder is spread a pixel at a time across
  // the keys, where nobody can see it. The Shape section's button grid is
  // measured from the left for the same reason.
  auto const minSpan = numViews * fingertipSize;
  auto const available
      = juce::jmax (minSpan, headerArea.getWidth () - headerGap * numHeaderGaps);

  auto const rowLeft = headerArea.getX ();
  auto const rowTop = headerArea.getY ();
  auto const rowHeight = headerArea.getHeight ();

  // The left edge of the view that begins after `units` views and `gaps`
  // gaps. At units == numViews it is the row's right edge exactly.
  auto const edgeAt = [rowLeft, available, headerGap] (int units, int gaps) {
    return rowLeft + gaps * headerGap + (available * units) / numViews;
  };
  auto const keyFrom = [rowTop, rowHeight] (int x0, int x1) {
    return juce::Rectangle<int>{ x0, rowTop, x1 - x0, rowHeight };
  };

  int units = 0;
  int gaps = 0;

  auto const takeView = [&units, &gaps, &edgeAt, &keyFrom] {
    auto const x0 = edgeAt (units, gaps);
    ++units;
    auto const key = keyFrom (x0, edgeAt (units, gaps));
    ++gaps;
    return key;
  };

  // The clip's own view leads the row: it is the one the others are
  // variations on.
  out.tabClip = takeView ();
  // Right of CLIP: the clip's movement, which stood beside its shape there.
  out.tabMotion = takeView ();
  out.tabAction = takeView ();
  out.tabBrowser = takeView ();

  // The two mixers side by side: the shown channel's strip (CHMIX), then the
  // whole mixer over the sphere (MAINMIX), which came down from the status
  // bar so both are opened from one place.
  out.tabMixer = takeView ();
  out.tabMainMix = takeView ();
  out.tabRecord = takeView ();
  out.tabController = takeView ();

  for (index_t slot = 0; slot < numPadSlots; ++slot)
    out.slotButtons[slot] = {};

  // The transport has left this row. It stands over the global strip now --
  // rec, stop, play and act belong to the device the way MENU and TAP do, and
  // taking them out of here is what leaves room for a fourth view of the clip.

  area.removeFromTop (
      juce::jmax (4, barHeaderGap.of (out.clipBounds.getHeight ())));

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
      34, juce::jmax (34, barButtonMax.of (out.clipBounds.getHeight ())),
      static_cast<int> (metrics.knobDiam * 1.35f));

  // Three columns, and which card stands in them depends on the page
  // (2026-09-26). CLIP: Shape (picker, picture), then dir and end, then the
  // lengths. MOTION: Motion across the first two, Elevation in the third.
  // REC: Shape, then the Record card across the other two. The section
  // indices stay as they were, which keeps every sub-index list intact.
  std::array<juce::Rectangle<int>, 3> columns;
  for (auto &column : columns)
    column = area.removeFromLeft (sectionW).reduced (gap / 2, 0);

  out.sectionCards[0] = columns[0];
  out.playCard = columns[1];
  out.lengthCard = columns[2];
  out.sectionCards[2] = columns[0].getUnion (columns[1]);
  out.sectionCards[1] = columns[2];

  auto globalArea = out.globalBounds.reduced (paddingH (), paddingV);

  // The readout goes in the band above the strip's card — the same band the
  // slot label and the tabs are on, so the bar reads across at one height.
  // Over the *global* strip because what it reports comes from either page,
  // and dropped into the card instead it would take a row the channel grid
  // needs: at the smallest skin sizes that collapsed its cells to five pixels.
  // The band the readout used to stand in. The readout is in the status bar
  // now -- a reading among readings -- and the four things you do to a clip
  // stand here instead, over the strip that also carries MENU, REC and TAP.
  // The whole strip, band included. The band held the transport and then the
  // readout before it; both have gone, so the card takes the height rather
  // than leaving an empty row above itself.
  auto const globalCard = globalArea;
  out.readout = {};

  out.sectionCards[3] = globalCard.reduced (gap / 2, 0);
  out.globalContent = sectionContentBounds (out.sectionCards[3]);

  // ── Shape: the picker over the picture ────────────────────────────
  //
  // Which clip, then what it looks like: the picker on top, the picture
  // taking the rest of the column. Both on CLIP and REC.
  {
    auto content = sectionContentBounds (out.sectionCards[0]);
    {
      auto title = content.removeFromTop (titleRowHeight (content, headerSize));
      // The lock takes the right end of the title row. A square, so it
      // reads as a mark rather than a word, and the row's own height, so
      // it is as big as anything else that is pressed in a hurry.
      out.sectionLocks[0] = title.removeFromRight (title.getHeight ());
      out.sectionLabels[0] = title;
    }

    auto const gap = juce::jmax (2, out.buttonHeight / 8);

    // The field names the *clip* -- the settings the slot is played with --
    // and the picture keeps its own name over it. Two names because they are
    // two things, and the two controls under a finger here change one each:
    // the picture swaps the figure, the field swaps the values.
    auto const fieldH = juce::jmin (content.getHeight (),
                                    juce::jmax (fingertipSize, out.buttonHeight));
    out.clipField = content.removeFromTop (fieldH);
    content.removeFromTop (juce::jmin (content.getHeight (), gap));

    // The name lies over the picture rather than under it: there when you
    // look for it, out of the way when you are reading the shape.
    out.trajectoryIcon = content;
    out.trajectoryName = content;
  }

  // ── dir and end: two fields that step on a tap ────────────────────
  //
  // The middle column on CLIP: dir on top, end under it, each one field that
  // steps through its list on a tap -- Fwd Rev Bnce Rnd, Loop Stop Paus. They
  // were a key per choice for a day; the maintainer wanted the toggles back.
  // As tall as the bar's other keys, centred in their half of the card.
  {
    auto content = sectionContentBounds (out.playCard);
    out.playLabel = content.removeFromTop (titleRowHeight (content, headerSize));

    auto const gap = juce::jmax (2, out.buttonHeight / 8);
    auto const halfH = (content.getHeight () - gap) / 2;
    auto const top = content.removeFromTop (halfH);
    content.removeFromTop (juce::jmin (content.getHeight (), gap));
    auto const bottom = content.removeFromTop (halfH);

    auto const fieldH = juce::jmin (
        halfH, juce::jmax (fingertipSize, out.buttonHeight));
    out.directionButton = top.withSizeKeepingCentre (top.getWidth (), fieldH);
    out.endActionButton
        = bottom.withSizeKeepingCentre (bottom.getWidth (), fieldH);
  }

  // ── the lengths, two by two ───────────────────────────────────────
  {
    auto content = sectionContentBounds (out.lengthCard);
    out.lengthLabel
        = content.removeFromTop (titleRowHeight (content, headerSize));

    auto const gap = juce::jmax (2, out.buttonHeight / 8);
    auto const keyW = (content.getWidth () - gap) / 2;
    auto const keyH = (content.getHeight () - gap) / 2;
    for (int i = 0; i < numSpeedButtons; ++i)
      {
        auto const column = i % 2;
        auto const row = i / 2;
        out.speedButtons[static_cast<size_t> (i)]
            = { content.getX () + column * (keyW + gap),
                content.getY () + row * (keyH + gap), keyW, keyH };
      }
  }

  // Shape's four sub-elements: the picture, the clip field, then dir and end.
  // The lengths are not values a finger turns, so they are not sub-elements.
  out.controls[0] = { out.trajectoryIcon, out.clipField, out.directionButton,
                      out.endActionButton };

  // ── Elevation ────────────────────────────────────────────────────────
  {
    auto content = sectionContentBounds (out.sectionCards[1]);
    {
      auto title = content.removeFromTop (titleRowHeight (content, headerSize));
      // The lock takes the right end of the title row. A square, so it
      // reads as a mark rather than a word, and the row's own height, so
      // it is as big as anything else that is pressed in a hurry.
      out.sectionLocks[1] = title.removeFromRight (title.getHeight ());
      out.sectionLabels[1] = title;
    }

    // Four knobs in two rows and nothing else: the picture that stood above
    // them is at the top of the global strip since 2026-09-26.
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

    // The two clips above; below, elv -- where the middle of the trajectory
    // sits -- with the sway that moves it on its right: the standing value
    // beside the movement over it, the way Motion pairs its knobs.
    auto const [clipTopArea, clipBottomArea] = split (row1);
    auto const [elvArea, swayArea] = split (row2);

    out.controls[1] = {
      textCell (clipTopArea, metrics.knobDiam),
      textCell (clipBottomArea, metrics.knobDiam),
      textCell (swayArea, metrics.knobDiam),
      textCell (elvArea, metrics.knobDiam), // 3 elv, left of the sway
    };
  }

  // ── Motion ───────────────────────────────────────────────────────────
  {
    auto content = sectionContentBounds (out.sectionCards[2]);
    {
      auto title = content.removeFromTop (titleRowHeight (content, headerSize));
      // The lock takes the right end of the title row. A square, so it
      // reads as a mark rather than a word, and the row's own height, so
      // it is as big as anything else that is pressed in a hurry.
      out.sectionLocks[2] = title.removeFromRight (title.getHeight ());
      out.sectionLabels[2] = title;
    }

    // Two rows of four across the MOTION page's two left columns, each
    // standing value beside the movement that works on it: rot spin reach
    // swell over sqzX strX sqzY strY. Grouping by what a control does is
    // what lets a hand find the right knob without reading the words. The
    // fade and the bias went to the REC page -- see below.
    //
    // Shared out rather than taken one after another. A skin can cut the bar
    // down (clipSettingsHeightScale), and a section that helps itself row by
    // row leaves the whole shortfall on the row at the top.
    auto const gapH = juce::jmax (2, content.getWidth () / 40);
    auto const gapV = juce::jmax (2, content.getHeight () / 20);

    constexpr int motionKnobRows = 2;
    auto const wanted = controlBoxHeightForFont (bodySize, metrics.knobDiam);
    auto const available
        = (content.getHeight () - (motionKnobRows - 1) * gapV) / motionKnobRows;
    auto const motionRowH = juce::jmax (1, juce::jmin (wanted, available));

    auto const topRow = content.removeFromTop (motionRowH);
    content.removeFromTop (gapV);
    auto const bottomRow = content.removeFromTop (motionRowH);

    auto const colW = (topRow.getWidth () - 3 * gapH) / 4;
    auto const cellIn = [colW, gapH] (juce::Rectangle<int> row, int column) {
      return juce::Rectangle<int> (row.getX () + column * (colW + gapH),
                                   row.getY (), colW, row.getHeight ());
    };

    auto const rotArea = cellIn (topRow, 0);
    auto const spinArea = cellIn (topRow, 1);
    auto const reachArea = cellIn (topRow, 2);
    auto const swellArea = cellIn (topRow, 3);
    auto const sqzXArea = cellIn (bottomRow, 0);
    auto const strXArea = cellIn (bottomRow, 1);
    auto const sqzYArea = cellIn (bottomRow, 2);
    auto const strYArea = cellIn (bottomRow, 3);

    // In reading order, which is also sub-index order for the first time.
    out.controls[2] = {
      textCell (rotArea, metrics.knobDiam),   // 0 rot
      textCell (spinArea, metrics.knobDiam),  // 1 spin
      textCell (reachArea, metrics.knobDiam), // 2 reach
      textCell (swellArea, metrics.knobDiam), // 3 swell
      textCell (sqzXArea, metrics.knobDiam),  // 4 sqzX
      textCell (strXArea, metrics.knobDiam),  // 5 strX
      textCell (sqzYArea, metrics.knobDiam),  // 6 sqzY
      textCell (strYArea, metrics.knobDiam),  // 7 strY
      {},                                     // 8 fade, on REC -- below
      {},                                     // 9 bias, on REC -- below
    };
  }

  // ── The REC page's card ──────────────────────────────────────────────
  //
  // Across both of the columns Elevation and Motion stand in on CLIP: the
  // rec mode on top, as the key it has always been, and the fade and the
  // bias under it, big. They keep Motion's sub-indices 8 and 9, so the
  // encoders and the take reach them as before; only where they are drawn
  // moved.
  {
    out.recordCard = out.playCard.getUnion (out.lengthCard);
    auto content = sectionContentBounds (out.recordCard);
    out.recordLabel = content.removeFromTop (
        titleRowHeight (content, headerSize));

    auto const gap = juce::jmax (2, out.buttonHeight / 4);
    auto keyRow = content.removeFromTop (
        juce::jmin (content.getHeight (), out.buttonHeight));
    out.recModeButton = keyRow.removeFromLeft (keyRow.getWidth () / 2);
    content.removeFromTop (juce::jmin (content.getHeight (), gap));

    auto const gapH = juce::jmax (2, content.getWidth () / 20);
    auto const half = (content.getWidth () - gapH) / 2;
    auto const fadeArea = content.removeFromLeft (half);
    content.removeFromLeft (gapH);
    auto const biasArea = content;

    out.controls[2][8] = textCell (fadeArea, metrics.knobDiam);
    out.controls[2][9] = textCell (biasArea, metrics.knobDiam);
  }

  // ── Global section ───────────────────────────────────────────────────
  {
    // No title: "global" named a panel whose contents name themselves -- the
    // faces wear their channels' colours and the keys carry words or marks.
    auto content = out.globalContent;
    out.sectionLabels[3] = {};

    // Top to bottom: the elevation picture, whose clip (the four faces), then
    // what to do to it (the transport, two by two) down to the foot. The 4x3 grid of 3D, FREQ
    // and Q went into the mixer strips and the six function keys went to the
    // status bar and the REC page, both on 2026-09-26; their room is the
    // transport's.
    auto const buttonRowH = out.buttonHeight;
    auto const buttonGap = juce::jmax (2, buttonRowH / 8);

    auto const frameInset = juce::jmax (2, content.getWidth () / 40);
    auto const blockGap = juce::jmax (4, buttonGap * 2);

    // ── the elevation picture ─────────────────────────────────────────
    //
    // At the head of the strip, over the faces: the side view of where the
    // shown clip sits and how high it may go, in a grey field of its own like
    // the faces and the transport -- a field that is touched to select it.
    // Never more than nine twentieths of the strip's height, so the faces and the
    // transport keep theirs.
    {
      auto const side = juce::jmin (content.getWidth (),
                                    content.getHeight () * 9 / 20);
      out.elevationFrame = content.removeFromTop (side);
      out.elevationGraphic = out.elevationFrame.reduced (frameInset);

      // The little camera in the top right corner, where the round picture
      // leaves room: an eighth of the frame, inset like the frame's content.
      auto const markSide = juce::jmax (1, out.elevationFrame.getWidth () / 8);
      out.elevationCameraMark
          = juce::Rectangle<int> (markSide, markSide)
                .withPosition (out.elevationFrame.getRight () - frameInset
                                   - markSide,
                               out.elevationFrame.getY () + frameInset);
      content.removeFromTop (juce::jmin (content.getHeight (), blockGap));
    }

    // ── the four channel faces ────────────────────────────────────────
    //
    // In a frame of their own, the way the transport stands in one: four
    // keys that choose *which clip* the bar describes, not what it does.
    // As tall as a function key, and never under a fingertip.
    {
      auto const faceH = juce::jmax (fingertipSize, buttonRowH);
      auto frame = content.removeFromTop (
          juce::jmin (content.getHeight (), faceH + 2 * frameInset));
      out.channelFacesFrame = frame;
      content.removeFromTop (juce::jmin (content.getHeight (), blockGap));

      auto faces = frame.reduced (frameInset);
      auto const faceGap = juce::jmax (2, faces.getWidth () / 60);
      auto const numFaces = static_cast<int> (numChannelColumns);
      auto const span = faces.getWidth () - (numFaces - 1) * faceGap;

      // Edges from the whole width, like the header's, so the last face ends
      // flush with the frame.
      for (int i = 0; i < numFaces; ++i)
        {
          auto const x0 = faces.getX () + i * faceGap + (span * i) / numFaces;
          auto const x1
              = faces.getX () + i * faceGap + (span * (i + 1)) / numFaces;
          out.channelFaces[static_cast<size_t> (i)]
              = { x0, faces.getY (), x1 - x0, faces.getHeight () };
        }
    }

    // ── the transport, two by two ─────────────────────────────────────
    //
    // Everything between the faces and the function keys, arranged as a
    // clip's pads are on PADS: play over act on the left, stop on the right,
    // and rec in the corner the pads give to Settings -- rec has no pad of
    // its own. Two rows rather than one because the grid's room is theirs
    // now, and a key the hand goes to mid-set is better big than wide.
    {
      out.transportFrame = content;
      auto keys = content.reduced (frameInset);

      auto const keyGap = juce::jmax (2, keys.getWidth () / 60);
      auto const keyW = (keys.getWidth () - keyGap) / 2;
      auto const keyH = (keys.getHeight () - keyGap) / 2;

      auto const cellOf = [] (TransportKey key) {
        switch (key)
          {
          case TransportKey::PlayPause: return std::pair{ 0, 0 };
          case TransportKey::Stop: return std::pair{ 1, 0 };
          case TransportKey::Action: return std::pair{ 0, 1 };
          case TransportKey::Record: return std::pair{ 1, 1 };
          }
        return std::pair{ 0, 0 };
      };

      for (int i = 0; i < numTransportKeys; ++i)
        {
          auto const [column, row]
              = cellOf (transportKeyOrder[static_cast<size_t> (i)]);
          out.transportButtons[static_cast<size_t> (i)]
              = { keys.getX () + column * (keyW + keyGap),
                  keys.getY () + row * (keyH + keyGap), keyW, keyH };
        }
    }

    // The rec mode's key stands on the REC page (above), and is still the
    // global section's one sub-element, so the encoder-era index does not
    // have to be special-cased away everywhere.
    out.controls[3] = { out.recModeButton };
  }

  return out;
}

}
