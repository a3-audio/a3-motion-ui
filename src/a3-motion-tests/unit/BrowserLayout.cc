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
  EXPECT_EQ (defaultBrowser ().listArea.getX (), 0);
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
// is read and edited there. It is as wide as it asks -- a script line of
// 85 characters fits without scrolling sideways -- and the list keeps the
// rest, full height.
TEST (BrowserLayout, EveryTabHasAScriptColumnAsWideAsItAsks)
{
  for (auto const list : { BrowserList::Sessions, BrowserList::Clips,
                           BrowserList::Shapes, BrowserList::Actions })
    {
      auto const l = defaultBrowser (list);
      EXPECT_EQ (l.detailArea.getWidth (), scriptWidth);
      EXPECT_EQ (l.listArea.getWidth (), defaultBrowser ().listArea.getWidth ())
          << "the columns stay put when the tab changes";
      EXPECT_EQ (l.detailArea.getRight (), sphere.getRight ());
      EXPECT_EQ (l.detailArea.getY (), sphere.getY ());
      EXPECT_EQ (l.detailArea.getBottom (), sphere.getBottom ());
      EXPECT_FALSE (l.listArea.intersects (l.detailArea));
      EXPECT_LT (l.listArea.getRight (), l.detailArea.getX ());
    }
}

// A column that asks for more than there is leaves the list enough for its
// keys and its tabs: three fingertips and the gaps between them.
TEST (BrowserLayout, AGreedyScriptLeavesTheListItsKeys)
{
  auto const l = layOutBrowser (sphere, buttonHeight, 12.f, BrowserList::Clips,
                                5000);
  for (auto const &key : { l.filterButton, l.renameButton, l.deleteButton })
    EXPECT_GE (key.getWidth (), fingertipSize);
  EXPECT_TRUE (sphere.contains (l.detailArea));
}

// The list's own keys stand at the top of its column, where back and close
// stood, in one row with the script's keys beside them: same top, same
// height. Load only on SETS -- a set is loaded on purpose, a clip by a tap.
TEST (BrowserLayout, TheKeysFormOneRowAtTheTop)
{
  for (auto const list : { BrowserList::Sessions, BrowserList::Clips })
    {
      auto const l = defaultBrowser (list);
      auto const panel = layOutScriptPanel (l.detailArea, buttonHeight, 0, 16);

      std::vector<juce::Rectangle<int>> keys{ l.filterButton, l.renameButton,
                                              l.deleteButton };
      if (list == BrowserList::Sessions)
        keys.insert (keys.begin (), l.loadButton);
      else
        EXPECT_TRUE (l.loadButton.isEmpty ());

      for (size_t i = 0; i < keys.size (); ++i)
        {
          EXPECT_GE (keys[i].getWidth (), fingertipSize);
          EXPECT_EQ (keys[i].getY (), panel.saveButton.getY ());
          EXPECT_EQ (keys[i].getHeight (), panel.saveButton.getHeight ());
          EXPECT_LE (keys[i].getRight (), l.detailArea.getX ());
          if (i > 0)
            EXPECT_LE (keys[i - 1].getRight (), keys[i].getX ());
        }
      EXPECT_TRUE (l.saveButton.isEmpty ()) << "the script carries Save";
      EXPECT_TRUE (l.saveAsButton.isEmpty ());
    }
}

// The four folders as two by two at the top of the list column, under its
// keys: SETS CLIPS over SVG ACTIONS, all one size, clear of the list.
TEST (BrowserLayout, TheFoldersStandTwoByTwoAboveTheList)
{
  auto const l = defaultBrowser ();

  EXPECT_EQ (l.setsTab.getY (), l.clipsTab.getY ());
  EXPECT_EQ (l.shapesTab.getY (), l.actionsTab.getY ());
  EXPECT_LT (l.setsTab.getBottom (), l.shapesTab.getY () + 1);
  EXPECT_LE (l.setsTab.getRight (), l.clipsTab.getX ());
  EXPECT_LE (l.shapesTab.getRight (), l.actionsTab.getX ());

  for (auto const &tab : { l.setsTab, l.clipsTab, l.shapesTab, l.actionsTab })
    {
      EXPECT_EQ (tab.getWidth (), l.clipsTab.getWidth ());
      EXPECT_EQ (tab.getHeight (), l.clipsTab.getHeight ());
      EXPECT_GE (tab.getWidth (), fingertipSize);
      EXPECT_GE (tab.getY (), l.filterButton.getBottom ());
      EXPECT_LE (tab.getBottom (), l.listArea.getY ());
      EXPECT_LE (tab.getRight (), l.detailArea.getX ());
    }
}
