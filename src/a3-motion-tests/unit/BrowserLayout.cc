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

TEST (BrowserLayout, EveryClipOnTheDeviceGetsAField)
{
  auto const l = defaultBrowser ();

  int count = 0;
  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      {
        EXPECT_FALSE (l.fields[channel][slot].isEmpty ())
            << "channel " << channel << " slot " << slot;
        ++count;
      }

  EXPECT_EQ (count, numBrowserFields);
}

TEST (BrowserLayout, TheFieldsAreLaidOutTheWayThePadsAre)
{
  // Channels across, slots down. If these two pages ever disagreed about which
  // box is which clip, a finger would put a clip somewhere it did not mean to.
  auto const l = defaultBrowser ();

  for (index_t channel = 0; channel + 1 < numChannelColumns; ++channel)
    EXPECT_LT (l.fields[channel][0].getX (), l.fields[channel + 1][0].getX ())
        << "channel " << channel;

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot + 1 < numPadSlots; ++slot)
      EXPECT_LT (l.fields[channel][slot].getY (),
                 l.fields[channel][slot + 1].getY ());
}

TEST (BrowserLayout, TheFieldsAndTheListDoNotOverlap)
{
  auto const l = defaultBrowser ();

  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      EXPECT_FALSE (l.fields[channel][slot].intersects (l.listArea))
          << "channel " << channel << " slot " << slot;
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

  ASSERT_FALSE (l.renameButton.isEmpty ());
  ASSERT_FALSE (l.saveSessionButton.isEmpty ());
  ASSERT_FALSE (l.loadSessionButton.isEmpty ());

  EXPECT_FALSE (l.renameButton.intersects (l.listArea));
  EXPECT_GE (l.renameButton.getY (), l.listArea.getBottom ());

  // Side by side, in reading order.
  EXPECT_LE (l.renameButton.getRight (), l.saveSessionButton.getX ());
  EXPECT_LE (l.saveSessionButton.getRight (), l.loadSessionButton.getX ());
}

TEST (BrowserLayout, AnEmptyAreaProducesNothingRatherThanNonsense)
{
  auto const l = layOutBrowser ({}, 34, 12.f);

  EXPECT_TRUE (l.rows.empty ());
  EXPECT_TRUE (l.listArea.isEmpty ());
  for (index_t channel = 0; channel < numChannelColumns; ++channel)
    for (index_t slot = 0; slot < numPadSlots; ++slot)
      EXPECT_TRUE (l.fields[channel][slot].isEmpty ());
}

// Scripts are their own files, so they are chosen where files are chosen: two
// words over the library say whether it is listing what a slot holds or what
// ACT does to it. Both have to be there and both have to be hittable, or the
// actions are a folder nobody can reach.
TEST (BrowserLayout, TheListSaysWhetherItShowsClipsOrActions)
{
  for (int width : { 480, 640, 768, 1024 })
    for (int height : { 160, 250, 400 })
      {
        auto const l = layOutBrowser ({ 0, 0, width, height }, 34, 14.f);

        EXPECT_FALSE (l.clipsTab.isEmpty ())
            << width << "x" << height;
        EXPECT_FALSE (l.actionsTab.isEmpty ())
            << width << "x" << height;

        // Side by side, not overlapping, and clear of the list they head.
        EXPECT_LE (l.clipsTab.getRight (), l.actionsTab.getX ())
            << width << "x" << height;
        EXPECT_LE (l.clipsTab.getBottom (), l.listArea.getY ())
            << width << "x" << height;
        EXPECT_LE (l.actionsTab.getBottom (), l.listArea.getY ())
            << width << "x" << height;

        // Over the library, not over the fields it is beside.
        EXPECT_GE (l.clipsTab.getX (), l.fields[0][0].getRight ())
            << width << "x" << height;
      }
}
