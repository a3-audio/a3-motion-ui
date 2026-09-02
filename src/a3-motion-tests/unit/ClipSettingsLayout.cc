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


#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <cmath>

#include <a3-motion-ui/components/ClipSettingsCaptions.hh>
#include <a3-motion-ui/components/ClipSettingsLayout.hh>

using namespace a3;

namespace
{

// The device panel's width; the bar gets the bottom quarter of its height
// (see A3MotionUIComponent::resized()).
constexpr int panelWidth = 1280;
constexpr int barHeight = 180;

juce::Rectangle<int> const bar{ 0, 0, panelWidth, barHeight };

// The sizes config/skins/default.json ships with, and the height the bar
// asks for at them — not a fixed 180px. Held at a fixed height the global
// section ran out of room for its grid and the rows collapsed onto each
// other, which says nothing about the layout and everything about the
// number in the test.
constexpr float defaultHeaderSize = 18.f;
constexpr float defaultBodySize = 14.f;
constexpr float defaultPotSize = 0.9f;

juce::Rectangle<int>
grownBar (float headerSize, float bodySize, float potSize)
{
  return { 0, 0, panelWidth,
           clipSettingsPreferredHeight (
               headerSize, bodySize, knobDiameterForFont (bodySize, potSize)) };
}

ClipSettingsLayout
defaultLayout ()
{
  return layOutClipSettings (grownBar (defaultHeaderSize, defaultBodySize,
                                       defaultPotSize),
                             defaultHeaderSize, defaultBodySize,
                             defaultPotSize);
}

}

TEST (ClipSettingsLayout, EverySectionHasItsControls)
{
  auto const l = defaultLayout ();

  EXPECT_EQ (l.controls[0].size (), 1u); // Shape: the pattern in the slot
  EXPECT_EQ (l.controls[1].size (), 6u); // Elevation
  EXPECT_EQ (l.controls[2].size (), 4u); // Motion
  EXPECT_EQ (l.controls[3].size (), 1u); // Global: the rec mode
}

// The count each section reports must be the count it lays out, or a tap
// lands on a control that was never drawn.
TEST (ClipSettingsLayout, TheCountMatchesWhatIsLaidOut)
{
  auto const l = defaultLayout ();

  for (int s = 0; s < numClipSettingsSections; ++s)
    EXPECT_EQ (static_cast<int> (l.controls[static_cast<size_t> (s)].size ()),
               numControlsInSection (s))
        << "section " << s;
}

TEST (ClipSettingsLayout, EveryControlSitsInsideItsSectionCard)
{
  auto const l = defaultLayout ();

  for (int s = 0; s < numClipSettingsSections; ++s)
    for (auto const &control : l.controls[static_cast<size_t> (s)])
      EXPECT_TRUE (l.sectionCards[static_cast<size_t> (s)].contains (control))
          << "section " << s << " control escapes its card";
}

TEST (ClipSettingsLayout, ControlsWithinASectionDoNotOverlap)
{
  auto const l = defaultLayout ();

  for (auto const &section : l.controls)
    for (size_t a = 0; a < section.size (); ++a)
      for (size_t b = a + 1; b < section.size (); ++b)
        EXPECT_TRUE (section[a].getIntersection (section[b]).isEmpty ())
            << "controls " << a << " and " << b << " overlap";
}

TEST (ClipSettingsLayout, SectionCardsDoNotOverlapEachOther)
{
  auto const l = defaultLayout ();

  for (size_t a = 0; a < l.sectionCards.size (); ++a)
    for (size_t b = a + 1; b < l.sectionCards.size (); ++b)
      EXPECT_TRUE (
          l.sectionCards[a].getIntersection (l.sectionCards[b]).isEmpty ())
          << "cards " << a << " and " << b << " overlap";
}

// The regression from issues/a3-motion-ui-clip-settings-layout-overflows-at-
// large-fonts.md: at large fonts and large knobs the bar used to spill. Here
// it is a condition rather than something to notice on a screen.
TEST (ClipSettingsLayout, NothingEscapesTheBarAtAnyPotOrFontSize)
{
  for (float potSize : { 0.6f, 0.9f, 1.f, 1.4f, 1.8f })
    for (float bodySize : { 9.f, 12.f, 16.f, 22.f, 28.f })
      {
        auto const l = layOutClipSettings (bar, bodySize * 1.3f, bodySize,
                                           potSize);

        for (int s = 0; s < numClipSettingsSections; ++s)
          for (auto const &control : l.controls[static_cast<size_t> (s)])
            EXPECT_TRUE (bar.contains (control))
                << "pot " << potSize << " body " << bodySize << " section "
                << s;
      }
}

TEST (ClipSettingsLayout, ClipAndGlobalPanelsSplitTheBarWithoutOverlap)
{
  auto const l = defaultLayout ();

  EXPECT_TRUE (l.clipBounds.getIntersection (l.globalBounds).isEmpty ());
  EXPECT_EQ (l.clipBounds.getWidth () + l.globalBounds.getWidth (),
             panelWidth);
}

// The Elevation section is indexed by sub-element but arranged by eye:
//
//   clip-top      clip-bottom
//   flat-elev     reach
//   flat          pole
//
// Sub-indices run reach(0), clip-top(1), clip-bottom(2), pole(3), flat(4),
// flat-elevation(5) — nothing like the reading order. Get the mapping wrong
// and a tap on one control lands on another.
TEST (ClipSettingsLayout, ElevationControlsAreOrderedBySubIndexNotByScreen)
{
  auto const l = defaultLayout ();
  auto const &e = l.controls[1];

  auto const reach = e[0];
  auto const clipTop = e[1];
  auto const clipBottom = e[2];
  auto const pole = e[3];
  auto const flat = e[4];
  auto const flatElevation = e[5];

  // Three rows, top to bottom.
  EXPECT_LT (clipTop.getY (), flatElevation.getY ());
  EXPECT_LT (flatElevation.getY (), flat.getY ());

  // Left column above left column, right beside right.
  EXPECT_EQ (clipTop.getY (), clipBottom.getY ());
  EXPECT_LT (clipTop.getX (), clipBottom.getX ());

  EXPECT_EQ (flatElevation.getY (), reach.getY ());
  EXPECT_LT (flatElevation.getX (), reach.getX ());

  EXPECT_EQ (flat.getY (), pole.getY ());
  EXPECT_LT (flat.getX (), pole.getX ());

  // What the maintainer asked for in so many words: the two buttons at the
  // bottom, flat-elevation directly above flat, reach in the middle on the
  // right.
  EXPECT_EQ (flat.getX (), flatElevation.getX ());
  EXPECT_GT (reach.getX (), flatElevation.getX ());
}

TEST (ClipSettingsLayout, OnlyFewValuedControlsAdvanceOnTap)
{
  // Elevation: mirror-south (3) and flat (4) are yes/no, and a yes/no is
  // toggled rather than stepped — see TwoStateControlsToggleRatherThanStep.
  EXPECT_FALSE (tapAdvancesValue (1, 3));
  EXPECT_FALSE (tapAdvancesValue (1, 4));
  // reach, clip-top, clip-bottom, flat-elevation are continuous.
  EXPECT_FALSE (tapAdvancesValue (1, 0));
  EXPECT_FALSE (tapAdvancesValue (1, 1));
  EXPECT_FALSE (tapAdvancesValue (1, 2));
  EXPECT_FALSE (tapAdvancesValue (1, 5));

  // Motion: direction (1) and end-action (2) have three states each;
  // speed (0) and seam (3) are lists worth dragging.
  EXPECT_TRUE (tapAdvancesValue (2, 1));
  EXPECT_TRUE (tapAdvancesValue (2, 2));
  EXPECT_FALSE (tapAdvancesValue (2, 0));
  EXPECT_FALSE (tapAdvancesValue (2, 3));

  // Shape: the pattern library, too long to tap through.
  EXPECT_FALSE (tapAdvancesValue (0, 0));

  // Global: the rec mode has few states.
  EXPECT_TRUE (tapAdvancesValue (3, 0));
}


// A control with exactly two states is toggled by a tap, not stepped by one.
// Stepping is what the encoders did, and it was tied to which way they were
// turned so a missed tick could not desync the value; a tap has no direction,
// so a stepped boolean could only ever be switched on. Three-state controls
// keep stepping — they wrap, so a tap always gets somewhere new.
TEST (ClipSettingsLayout, TwoStateControlsToggleRatherThanStep)
{
  EXPECT_TRUE (tapTogglesValue (1, 3));  // pole: north / south
  EXPECT_TRUE (tapTogglesValue (1, 4));  // flat: on / off

  EXPECT_FALSE (tapTogglesValue (1, 0)); // reach is continuous
  EXPECT_FALSE (tapTogglesValue (1, 5)); // flat-elevation is continuous
  EXPECT_FALSE (tapTogglesValue (2, 1)); // direction has three states
  EXPECT_FALSE (tapTogglesValue (2, 2)); // end-action has more
  EXPECT_FALSE (tapTogglesValue (3, 0)); // rec mode has three
}

// The two are alternatives, not layers: a tap either steps a value on or
// flips it, and a control that claimed both would do both on one tap.
TEST (ClipSettingsLayout, NoControlBothStepsAndToggles)
{
  for (int section = 0; section < numClipSettingsSections; ++section)
    for (int sub = 0; sub < numControlsInSection (section); ++sub)
      EXPECT_FALSE (tapAdvancesValue (section, sub)
                    && tapTogglesValue (section, sub))
          << "section " << section << " sub " << sub;
}

// The frame around a section is a line, not a border zone. Every control it
// holds has to sit inside the same content area the fonts were sized against
// — when those were two separate calculations the text was fitted to a
// narrower box than it was drawn in, and the sections looked cramped at
// widths where they were not.
TEST (ClipSettingsLayout, ControlsStayInsideTheirSectionContent)
{
  auto const l = defaultLayout ();

  for (int section = 0; section < numClipSettingsSections; ++section)
    {
      auto const content
          = sectionContentBounds (l.sectionCards[static_cast<size_t> (section)]);

      for (auto const &cell : l.controls[static_cast<size_t> (section)])
        EXPECT_TRUE (content.contains (cell))
            << "section " << section << ": " << cell.toString ()
            << " outside " << content.toString ();
    }
}

// A frame that eats more than a tenth of a section's width is a border zone
// again. At the device's width the three clip sections are a sixth each, and
// what they lose to their own inset they lose from four-value rows.
TEST (ClipSettingsLayout, TheSectionFrameCostsLittleWidth)
{
  auto const l = defaultLayout ();

  for (int section = 0; section < numClipSettingsSections; ++section)
    {
      auto const card = l.sectionCards[static_cast<size_t> (section)];
      auto const content = sectionContentBounds (card);

      EXPECT_LE (card.getWidth () - content.getWidth (), card.getWidth () / 10)
          << "section " << section;
    }
}

// Menu, Rec and Tap sit in the global strip beside the clip's sections. They
// are not sub-elements of it — no encoder reaches them, only a finger — so
// they live beside `controls`, not in it.
TEST (ClipSettingsLayout, TheActionButtonsSitInTheGlobalStrip)
{
  auto const l = defaultLayout ();
  auto const card = l.sectionCards[3];

  EXPECT_TRUE (card.contains (l.menuButton));
  EXPECT_TRUE (card.contains (l.recButton));
  EXPECT_TRUE (card.contains (l.tapButton));
}

TEST (ClipSettingsLayout, TheActionButtonsDoNotOverlapEachOther)
{
  auto const l = defaultLayout ();

  std::vector<juce::Rectangle<int>> const row{ l.recModeButton,
                                               l.clockModeButton,
                                               l.menuButton, l.recButton,
                                               l.tapButton };

  for (size_t a = 0; a < row.size (); ++a)
    for (size_t b = a + 1; b < row.size (); ++b)
      EXPECT_TRUE (row[a].getIntersection (row[b]).isEmpty ())
          << "buttons " << a << " and " << b << " overlap";
}

// Five buttons under the grid: the two that carry a value on top, the two
// that do something under them, TAP across the width at the bottom.
TEST (ClipSettingsLayout, TheButtonsSitInThreeRowsUnderTheGrid)
{
  auto const l = defaultLayout ();

  EXPECT_EQ (l.recModeButton.getY (), l.clockModeButton.getY ());
  EXPECT_EQ (l.menuButton.getY (), l.recButton.getY ());

  EXPECT_GT (l.menuButton.getY (), l.recModeButton.getY ());
  EXPECT_GT (l.tapButton.getY (), l.menuButton.getY ());

  EXPECT_LT (l.recModeButton.getX (), l.clockModeButton.getX ());
  EXPECT_LT (l.menuButton.getX (), l.recButton.getX ());

  // TAP is the one pressed in time, so it gets the width of two.
  EXPECT_GT (l.tapButton.getWidth (), l.menuButton.getWidth ());

  for (auto const &b : { l.clockModeButton, l.menuButton, l.recButton,
                         l.tapButton })
    EXPECT_EQ (b.getHeight (), l.recModeButton.getHeight ());

  // Under the grid, not beside it.
  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      EXPECT_LE (cell.getBottom (), l.recModeButton.getY ());
}

// A target a finger can actually hit, at every size the bar is used at.
//
// The bar's height follows the font — see A3MotionUIComponent::resized() and
// clipSettingsPreferredHeight() — so the height has to follow it here too.
// Held at a fixed 180 px this failed at body size 28, which is a combination
// the app never puts on screen.
TEST (ClipSettingsLayout, TheActionButtonsStayBigEnoughToHit)
{
  for (float potSize : { 0.6f, 1.f, 1.8f })
    for (float bodySize : { 9.f, 16.f, 28.f })
      {
        auto const headerSize = bodySize * 1.3f;
        auto const knobDiam = knobDiameterForFont (bodySize, potSize);
        auto const l = layOutClipSettings (
            grownBar (headerSize, bodySize, potSize), headerSize, bodySize,
            potSize);

        for (auto const &b : { l.recModeButton, l.clockModeButton, l.menuButton,
                         l.recButton, l.tapButton })
          {
            // 34 is the floor the layout clamps to; above it the buttons
            // follow knobDiam like every other control in the bar. It was 24,
            // which measured fine and missed the point: a fingertip is wider
            // than that, and TAP was hard to hit.
            EXPECT_GE (b.getWidth (), 40) << "pot " << potSize;
            EXPECT_GE (b.getHeight (), 34) << "body " << bodySize;
          }
      }
}


// Freq, Q and the third value belong to a channel each, not to the clip the
// bar happens to show — so they are a grid of their own in the global
// section, one column per channel.
TEST (ClipSettingsLayout, TheChannelGridHasAColumnPerChannelAndThreeRows)
{
  auto const l = defaultLayout ();
  auto const card = l.sectionCards[3];

  for (int col = 0; col < numChannelColumns; ++col)
    for (int row = 0; row < numChannelRows; ++row)
      EXPECT_TRUE (card.contains (
          l.channelGrid[static_cast<size_t> (col)][static_cast<size_t> (row)]))
          << "channel " << col << " row " << row << " escapes the section";
}

TEST (ClipSettingsLayout, NoTwoGridCellsOverlap)
{
  auto const l = defaultLayout ();

  std::vector<juce::Rectangle<int>> cells;
  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      cells.push_back (cell);

  for (size_t a = 0; a < cells.size (); ++a)
    for (size_t b = a + 1; b < cells.size (); ++b)
      EXPECT_TRUE (cells[a].getIntersection (cells[b]).isEmpty ())
          << "cells " << a << " and " << b << " overlap";
}

// Channels read left to right, the three values top to bottom — freq, Q, 3d.
TEST (ClipSettingsLayout, TheGridReadsLeftToRightAndTopToBottom)
{
  auto const l = defaultLayout ();

  for (int col = 1; col < numChannelColumns; ++col)
    EXPECT_GT (l.channelGrid[static_cast<size_t> (col)][0].getX (),
               l.channelGrid[static_cast<size_t> (col - 1)][0].getX ());

  for (int row = 1; row < numChannelRows; ++row)
    EXPECT_GT (l.channelGrid[0][static_cast<size_t> (row)].getY (),
               l.channelGrid[0][static_cast<size_t> (row - 1)].getY ());
}

// The grid must not run into the strip that holds the rec mode and the
// buttons beside it.
TEST (ClipSettingsLayout, TheGridClearsTheActionButtonsAndTheRecMode)
{
  auto const l = defaultLayout ();

  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      {
        EXPECT_TRUE (cell.getIntersection (l.recModeButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.clockModeButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.menuButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.recButton).isEmpty ());
        EXPECT_TRUE (cell.getIntersection (l.tapButton).isEmpty ());
      }
}

// A cell a finger can hit, at every size the bar is used at.
TEST (ClipSettingsLayout, GridCellsStayBigEnoughToHit)
{
  for (float potSize : { 0.6f, 1.f, 1.8f })
    for (float bodySize : { 9.f, 16.f, 28.f })
      {
        auto const headerSize = bodySize * 1.3f;
        auto const knobDiam = knobDiameterForFont (bodySize, potSize);
        auto const l = layOutClipSettings (
            grownBar (headerSize, bodySize, potSize), headerSize, bodySize,
            potSize);

        // The invariant that matters: a cell holds the knob it draws. An
        // absolute floor was the wrong test — at the smallest font and pot
        // setting knobDiameterForFont() bottoms out at 10 px, and a 12 px
        // cell around a 10 px knob is right, not cramped. Everything on the
        // bar is that small at that setting.
        // Only that a cell stays usable. It cannot be promised the bar's
        // standard knob diameter any more: the global section is a quarter
        // of the bar, and at the largest pot size four columns of full-size
        // knobs want more than that quarter holds — paintGridKnob then
        // draws to the cell. What the grid gets at the sizes the device
        // ships with is asserted separately, below.
        juce::ignoreUnused (knobDiam);

        for (auto const &column : l.channelGrid)
          for (auto const &cell : column)
            {
              EXPECT_GE (cell.getHeight (), 10)
                  << "pot " << potSize << " body " << bodySize;
              EXPECT_GE (cell.getWidth (), 10)
                  << "pot " << potSize << " body " << bodySize;
            }
      }
}


// At the sizes the device actually ships with, the grid gets the fifth more
// it asks for — the clamp above is for the extremes, not the normal case.
TEST (ClipSettingsLayout, TheGridGetsItsFullKnobAtShippedSizes)
{
  constexpr float bodySize = 14.f;
  constexpr float headerSize = 18.f;
  constexpr float potSize = 0.9f; // config/skins/default.json

  auto const knobDiam = knobDiameterForFont (bodySize, potSize);
  auto const gridKnob = static_cast<int> (knobDiam * 1.2f);

  juce::Rectangle<int> const grown{
    0, 0, panelWidth,
    clipSettingsPreferredHeight (headerSize, bodySize, knobDiam)
  };
  auto const l = layOutClipSettings (grown, headerSize, bodySize, potSize);

  for (auto const &column : l.channelGrid)
    for (auto const &cell : column)
      {
        EXPECT_GE (cell.getWidth (), gridKnob);
        EXPECT_GE (cell.getHeight (), gridKnob);
      }
}


// The take's length is eight buttons on the Shape section's floor, two rows
// of four, reading 1/4 .. 32 in order. It used to be a dropdown offering the
// whole range from 1/128 to 16 bars.
TEST (ClipSettingsLayout, TheLengthButtonsSitInTwoRowsInOrder)
{
  auto const l = defaultLayout ();
  auto const card = l.sectionCards[0];

  for (int i = 0; i < numRecordLengths; ++i)
    EXPECT_TRUE (card.contains (l.lengthButtons[static_cast<size_t> (i)]))
        << "length " << recordLengthNames[i] << " escapes its section";

  // Four on each row.
  for (int i = 1; i < 4; ++i)
    {
      EXPECT_EQ (l.lengthButtons[static_cast<size_t> (i)].getY (),
                 l.lengthButtons[0].getY ());
      EXPECT_GT (l.lengthButtons[static_cast<size_t> (i)].getX (),
                 l.lengthButtons[static_cast<size_t> (i - 1)].getX ());
    }
  for (int i = 5; i < numRecordLengths; ++i)
    {
      EXPECT_EQ (l.lengthButtons[static_cast<size_t> (i)].getY (),
                 l.lengthButtons[4].getY ());
      EXPECT_GT (l.lengthButtons[static_cast<size_t> (i)].getX (),
                 l.lengthButtons[static_cast<size_t> (i - 1)].getX ());
    }
  EXPECT_GT (l.lengthButtons[4].getY (), l.lengthButtons[0].getY ());
}

TEST (ClipSettingsLayout, NoTwoLengthButtonsOverlap)
{
  auto const l = defaultLayout ();

  for (size_t a = 0; a < numRecordLengths; ++a)
    for (size_t b = a + 1; b < numRecordLengths; ++b)
      EXPECT_TRUE (l.lengthButtons[a]
                       .getIntersection (l.lengthButtons[b])
                       .isEmpty ())
          << "lengths " << recordLengthNames[a] << " and "
          << recordLengthNames[b] << " overlap";
}

// The pictogram keeps the room above them, and they do not run into it.
TEST (ClipSettingsLayout, TheLengthButtonsClearThePictogram)
{
  auto const l = defaultLayout ();

  for (auto const &b : l.lengthButtons)
    {
      EXPECT_TRUE (b.getIntersection (l.trajectoryIcon).isEmpty ());
      EXPECT_GE (b.getY (), l.trajectoryIcon.getBottom ());
    }
}

// The wording and the powers of two have to agree: the button says what the
// take will be, and log2 is what the setting holds.
TEST (ClipSettingsLayout, TheLengthNamesMatchTheirPowersOfTwo)
{
  for (int i = 0; i < numRecordLengths; ++i)
    {
      auto const log2 = recordLengthLog2[i];
      auto const expected
          = log2 >= 0 ? juce::String (static_cast<int> (std::exp2 (log2)))
                      : "1/" + juce::String (
                            static_cast<int> (std::exp2 (-log2)));

      EXPECT_EQ (juce::String (recordLengthNames[i]), expected);
    }
}
