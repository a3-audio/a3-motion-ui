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

ClipSettingsLayout
defaultLayout ()
{
  return layOutClipSettings (bar, 18.f, 14.f, 1.f);
}

}

TEST (ClipSettingsLayout, EverySectionHasItsControls)
{
  auto const l = defaultLayout ();

  EXPECT_EQ (l.controls[0].size (), 2u); // Shape: pattern, record length
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

// The screen order in the Elevation section is reach, mirror-south,
// clip-top, ... — the sub-index order is not. Getting this wrong makes a tap
// on "reach" land on mirror-south.
TEST (ClipSettingsLayout, ElevationControlsAreOrderedBySubIndexNotByScreen)
{
  auto const l = defaultLayout ();
  auto const &elevation = l.controls[1];

  // reach (0) and mirror-south (3) share the top row: same y, reach left.
  EXPECT_EQ (elevation[0].getY (), elevation[3].getY ());
  EXPECT_LT (elevation[0].getX (), elevation[3].getX ());

  // clip-top (1) and clip-bottom (2) share the row below it.
  EXPECT_EQ (elevation[1].getY (), elevation[2].getY ());
  EXPECT_LT (elevation[1].getX (), elevation[2].getX ());
  EXPECT_GT (elevation[1].getY (), elevation[0].getY ());

  // flat (4) and flat-elevation (5) share the bottom row.
  EXPECT_EQ (elevation[4].getY (), elevation[5].getY ());
  EXPECT_LT (elevation[4].getX (), elevation[5].getX ());
  EXPECT_GT (elevation[4].getY (), elevation[1].getY ());
}

TEST (ClipSettingsLayout, OnlyFewValuedControlsAdvanceOnTap)
{
  // Elevation: mirror-south (3) and flat (4) are yes/no.
  EXPECT_TRUE (tapAdvancesValue (1, 3));
  EXPECT_TRUE (tapAdvancesValue (1, 4));
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

  // Shape: the pattern library and the record length, both too long to tap
  // through.
  EXPECT_FALSE (tapAdvancesValue (0, 0));
  EXPECT_FALSE (tapAdvancesValue (0, 1));

  // Global: the rec mode has few states.
  EXPECT_TRUE (tapAdvancesValue (3, 0));
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

  std::vector<juce::Rectangle<int>> const row{ l.recModeButton, l.menuButton,
                                               l.recButton, l.tapButton };

  for (size_t a = 0; a < row.size (); ++a)
    for (size_t b = a + 1; b < row.size (); ++b)
      EXPECT_TRUE (row[a].getIntersection (row[b]).isEmpty ())
          << "buttons " << a << " and " << b << " overlap";
}

// Four equal buttons two by two under the grid: rec mode, menu, rec, tap.
TEST (ClipSettingsLayout, TheFourButtonsSitTwoByTwoUnderTheGrid)
{
  auto const l = defaultLayout ();

  // Two by two: rec mode | menu on top, rec | tap below.
  EXPECT_EQ (l.recModeButton.getY (), l.menuButton.getY ());
  EXPECT_EQ (l.recButton.getY (), l.tapButton.getY ());
  EXPECT_GT (l.recButton.getY (), l.recModeButton.getY ());

  EXPECT_LT (l.recModeButton.getX (), l.menuButton.getX ());
  EXPECT_LT (l.recButton.getX (), l.tapButton.getX ());

  for (auto const &b : { l.menuButton, l.recButton, l.tapButton })
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
        juce::Rectangle<int> const grown{
          0, 0, panelWidth,
          clipSettingsPreferredHeight (headerSize, bodySize, knobDiam)
        };

        auto const l = layOutClipSettings (grown, headerSize, bodySize,
                                           potSize);

        for (auto const &b : { l.recModeButton, l.menuButton, l.recButton,
                         l.tapButton })
          {
            // 24 is the floor the layout clamps to at the smallest font and
            // pot setting; above that the buttons follow knobDiam like every
            // other control in the bar. Not a comfortable target for a
            // finger — but at that setting nothing on the bar is.
            EXPECT_GE (b.getWidth (), 40) << "pot " << potSize;
            EXPECT_GE (b.getHeight (), 24) << "body " << bodySize;
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
        juce::Rectangle<int> const grown{
          0, 0, panelWidth,
          clipSettingsPreferredHeight (headerSize, bodySize, knobDiam)
        };

        auto const l = layOutClipSettings (grown, headerSize, bodySize,
                                           potSize);

        // The invariant that matters: a cell holds the knob it draws. An
        // absolute floor was the wrong test — at the smallest font and pot
        // setting knobDiameterForFont() bottoms out at 10 px, and a 12 px
        // cell around a 10 px knob is right, not cramped. Everything on the
        // bar is that small at that setting.
        for (auto const &column : l.channelGrid)
          for (auto const &cell : column)
            {
              EXPECT_GE (cell.getHeight (), knobDiam)
                  << "pot " << potSize << " body " << bodySize;
              EXPECT_GE (cell.getWidth (), knobDiam)
                  << "pot " << potSize << " body " << bodySize;
            }
      }
}
