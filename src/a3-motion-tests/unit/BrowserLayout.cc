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

#include <a3-motion-ui/components/BrowserLayout.hh>
#include <a3-motion-ui/components/ScriptPanelLayout.hh>

using namespace a3;

namespace
{
// FILES over the sphere on the device: the width, and the height the sphere
// leaves it. The script column asks for what 86 characters of the panel's
// font take (BrowserComponent measures it).
juce::Rectangle<int> const sphere{ 0, 0, 768, 620 };
constexpr int buttonHeight = 36;
constexpr int scriptWidth = 520;

BrowserLayout
defaultBrowser (BrowserList list = BrowserList::Clips)
{
  return layOutBrowser (sphere, buttonHeight, 12.f, list, scriptWidth);
}
}

// The eight destination fields are gone: the channel faces in the header
// choose what is filled, for every view at once, and the list is what the
// page is for.
TEST (BrowserLayout, ThereAreNoDestinationFieldsAnyMore)
{
  // Nothing left of the list's tile; the list stands in it.
  EXPECT_EQ (defaultBrowser ().listTile.getX (), 0);
}

TEST (BrowserLayout, EveryListRowIsBigEnoughToHit)
{
  // A library row is tapped in the dark by a hand that is also doing something
  // else. Forty patterns squeezed into the area would be four pixels each, so
  // the list shows as many as fit and scrolls for the rest.
  for (int height : { 300, 400, 520, 620 })
    {
      auto const l = layOutBrowser ({ 0, 0, 768, height }, buttonHeight, 12.f,
                                    BrowserList::Clips, scriptWidth);

      EXPECT_GE (l.rowHeight, fingertipSize) << "height " << height;
      EXPECT_GE (l.visibleRows, 1) << "height " << height;

      for (auto const &row : l.rows)
        {
          EXPECT_GE (row.getHeight (), fingertipSize) << "height " << height;
          EXPECT_TRUE (l.listArea.contains (row)) << "height " << height;
        }
    }
}

TEST (BrowserLayout, TheRowsDoNotOverlapEachOther)
{
  auto const l = defaultBrowser ();

  for (size_t i = 0; i + 1 < l.rows.size (); ++i)
    EXPECT_LE (l.rows[i].getBottom (), l.rows[i + 1].getY ()) << "row " << i;
}

TEST (BrowserLayout, AnEmptyAreaProducesNothingRatherThanNonsense)
{
  auto const l = layOutBrowser ({}, buttonHeight, 12.f, BrowserList::Clips,
                                scriptWidth);

  EXPECT_TRUE (l.rows.empty ());
  EXPECT_TRUE (l.listArea.isEmpty ());
  EXPECT_TRUE (l.detailArea.isEmpty ());
  EXPECT_TRUE (l.clipsTab.isEmpty ());
  EXPECT_TRUE (l.setsTab.isEmpty ());
}

// Every tab has the script column (2026-09-27, evening): every file in FILES
// is read and edited there. It stands on a tile of its own, as wide as it
// asks -- a script line of 85 characters fits without scrolling sideways --
// and the list keeps the rest, on its own tile.
TEST (BrowserLayout, EveryTabHasAScriptColumnAsWideAsItAsks)
{
  for (auto const list : { BrowserList::Sessions, BrowserList::Clips,
                           BrowserList::Shapes, BrowserList::Actions })
    {
      auto const l = defaultBrowser (list);
      EXPECT_EQ (l.detailArea.getWidth (), scriptWidth);
      EXPECT_TRUE (l.detailTile.contains (l.detailArea));
      EXPECT_EQ (l.detailTile.getRight (), sphere.getRight ());
      EXPECT_EQ (l.detailTile.getY (), sphere.getY ());
      EXPECT_EQ (l.detailTile.getBottom (), sphere.getBottom ());
      EXPECT_FALSE (l.listTile.intersects (l.detailTile));
      EXPECT_TRUE (l.listTile.contains (l.listArea));
      EXPECT_EQ (l.listArea.getWidth (), defaultBrowser ().listArea.getWidth ())
          << "the columns stay put when the tab changes";
    }
}

// A column that asks for more than there is leaves the list enough for a
// row of three keys.
TEST (BrowserLayout, AGreedyScriptLeavesTheListItsKeys)
{
  auto const l = layOutBrowser (sphere, buttonHeight, 12.f, BrowserList::Sessions,
                                5000);
  for (auto const &key : { l.renameButton, l.deleteButton, l.loadButton,
                           l.cancelButton, l.saveButton, l.saveAsButton })
    EXPECT_GE (key.getWidth (), fingertipSize);
  EXPECT_TRUE (sphere.contains (l.detailTile));
}

// Down the list's tile (maintainer, 2026-09-27): the four folders two by
// two, FROM, the filter, the list, and the other keys at its foot.
TEST (BrowserLayout, TheListTileReadsTopToBottom)
{
  auto const l = defaultBrowser ();

  EXPECT_EQ (l.setsTab.getY (), l.clipsTab.getY ());
  EXPECT_EQ (l.shapesTab.getY (), l.actionsTab.getY ());
  EXPECT_LE (l.setsTab.getBottom (), l.shapesTab.getY ());
  EXPECT_LE (l.setsTab.getRight (), l.clipsTab.getX ());
  for (auto const &tab : { l.setsTab, l.clipsTab, l.shapesTab, l.actionsTab })
    {
      EXPECT_EQ (tab.getWidth (), l.clipsTab.getWidth ());
      EXPECT_GE (tab.getWidth (), fingertipSize);
      EXPECT_TRUE (l.listTile.contains (tab));
    }

  EXPECT_GE (l.fromClipButton.getY (), l.shapesTab.getBottom ());
  EXPECT_GE (l.filterButton.getY (), l.fromClipButton.getBottom ());
  EXPECT_GE (l.listArea.getY (), l.filterButton.getBottom ());
  EXPECT_EQ (l.fromClipButton.getWidth (), l.listArea.getWidth ());
  EXPECT_EQ (l.filterButton.getWidth (), l.listArea.getWidth ());

  for (auto const &key : { l.renameButton, l.deleteButton, l.cancelButton,
                           l.saveButton, l.saveAsButton })
    {
      EXPECT_GE (key.getY (), l.listArea.getBottom ()) << "at the foot";
      EXPECT_GE (key.getWidth (), fingertipSize);
      EXPECT_GE (key.getHeight (), fingertipSize);
      EXPECT_TRUE (l.listTile.contains (key));
    }
  // Rename and Delete over Cancel, Save and Save as.
  EXPECT_LE (l.renameButton.getBottom (), l.saveButton.getY ());
  EXPECT_EQ (l.cancelButton.getY (), l.saveAsButton.getY ());
  EXPECT_LE (l.cancelButton.getRight (), l.saveButton.getX ());
  EXPECT_LE (l.saveButton.getRight (), l.saveAsButton.getX ());
}

// Load only on SETS -- a set is loaded on purpose, a clip by a tap.
TEST (BrowserLayout, LoadStandsOnlyOnSets)
{
  EXPECT_FALSE (defaultBrowser (BrowserList::Sessions).loadButton.isEmpty ());
  EXPECT_TRUE (defaultBrowser (BrowserList::Clips).loadButton.isEmpty ());
  auto const sets = defaultBrowser (BrowserList::Sessions);
  EXPECT_EQ (sets.loadButton.getY (), sets.renameButton.getY ());
}
