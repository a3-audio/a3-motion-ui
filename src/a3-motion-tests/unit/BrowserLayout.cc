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

using namespace a3;

namespace
{
BrowserLayout
defaultBrowser ()
{
  return layOutBrowser ({ 0, 0, 578, 250 }, 34, 12.f);
}
}

// The eight destination fields are gone.
//
// They laid the device's clips out the way the pads page does, and you chose
// one before choosing what to put in it. The four channel faces in the header
// do that now, for every view of the settings area at once -- so the browser
// keeping a second idea of which clip you were filling meant two selections
// that could point at different slots, and the bar telling you about one while
// the list filled the other. It fills whatever the faces have chosen.
TEST (BrowserLayout, ThereAreNoDestinationFieldsAnyMore)
{
  auto const l = defaultBrowser ();

  // The list gets the room they used to take: it is what the page is for.
  EXPECT_GE (l.listArea.getWidth (), defaultBrowser ().listArea.getWidth ());
  EXPECT_EQ (l.listArea.getX (), 0);
}

TEST (BrowserLayout, EveryListRowIsBigEnoughToHit)
{
  // A library row is tapped in the dark by a hand that is also doing something
  // else. Forty patterns squeezed into the area would be four pixels each, so
  // the list shows as many as fit and scrolls for the rest.
  for (int height : { 160, 200, 250, 320, 400 })
    {
      auto const l = layOutBrowser ({ 0, 0, 578, height }, 34, 12.f);

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

TEST (BrowserLayout, TheActionStripSitsUnderTheListAndNotInIt)
{
  auto const l = defaultBrowser ();

  ASSERT_FALSE (l.filterButton.isEmpty ());
  ASSERT_FALSE (l.renameButton.isEmpty ());
  ASSERT_FALSE (l.saveButton.isEmpty ());
  ASSERT_FALSE (l.deleteButton.isEmpty ());

  EXPECT_FALSE (l.renameButton.intersects (l.listArea));
  EXPECT_GE (l.renameButton.getY (), l.listArea.getBottom ());

  // Side by side, in reading order.
  // The filter first -- it changes what is listed, the other three act on a
  // row of it -- then left to right in the order they are reached for.
  EXPECT_LE (l.filterButton.getRight (), l.renameButton.getX ());
  EXPECT_LE (l.renameButton.getRight (), l.saveButton.getX ());
  EXPECT_LE (l.saveButton.getRight (), l.deleteButton.getX ());

  // Four keys of one width. One narrower than its neighbours reads as a
  // different kind of thing, and these are all keys.
  EXPECT_EQ (l.filterButton.getWidth (), l.renameButton.getWidth ());
  EXPECT_EQ (l.renameButton.getWidth (), l.saveButton.getWidth ());

  EXPECT_FALSE (l.filterButton.intersects (l.listArea));
}

TEST (BrowserLayout, AnEmptyAreaProducesNothingRatherThanNonsense)
{
  auto const l = layOutBrowser ({}, 34, 12.f);

  EXPECT_TRUE (l.rows.empty ());
  EXPECT_TRUE (l.listArea.isEmpty ());
  EXPECT_TRUE (l.clipsTab.isEmpty ());
  EXPECT_TRUE (l.setsTab.isEmpty ());
}

// Three kinds of file, three words over the one list: what a slot holds, what
// ACT does to it, and the arrangement of all eight together. A set used to be
// reached by touching the strip that said which set was loaded -- a control
// that looked like a label, in a corner of the page that has now gone. It is a
// tab like the other two, because it is the same question asked of a third
// folder.
TEST (BrowserLayout, TheListSaysWhichOfTheThreeFoldersItShows)
{
  for (int width : { 480, 640, 768, 1024 })
    for (int height : { 160, 250, 400 })
      {
        auto const l = layOutBrowser ({ 0, 0, width, height }, 34, 14.f);

        ASSERT_FALSE (l.clipsTab.isEmpty ()) << width << "x" << height;
        ASSERT_FALSE (l.actionsTab.isEmpty ()) << width << "x" << height;
        ASSERT_FALSE (l.setsTab.isEmpty ()) << width << "x" << height;

        // Side by side, in reading order, none of them overlapping.
        EXPECT_LE (l.clipsTab.getRight (), l.actionsTab.getX ())
            << width << "x" << height;
        EXPECT_LE (l.actionsTab.getRight (), l.setsTab.getX ())
            << width << "x" << height;

        // Clear of the list they head.
        for (auto const &tab : { l.clipsTab, l.actionsTab, l.setsTab })
          {
            EXPECT_LE (tab.getBottom (), l.listArea.getY ())
                << width << "x" << height;
            EXPECT_GE (tab.getHeight (), 1) << width << "x" << height;
          }

        // One row of three, all the same width: they ask one question of
        // three folders, so no one of them may look like the main one.
        EXPECT_EQ (l.actionsTab.getWidth (), l.clipsTab.getWidth ())
            << width << "x" << height;
        EXPECT_EQ (l.setsTab.getWidth (), l.clipsTab.getWidth ())
            << width << "x" << height;
        EXPECT_EQ (l.actionsTab.getY (), l.clipsTab.getY ())
            << width << "x" << height;
        EXPECT_EQ (l.setsTab.getY (), l.clipsTab.getY ())
            << width << "x" << height;
      }
}
