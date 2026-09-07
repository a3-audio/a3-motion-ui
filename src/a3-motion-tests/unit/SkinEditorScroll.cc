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

#include <a3-motion-ui/components/ListScroll.hh>

using namespace a3;

// A long list under a short window. The window used to be placed around the
// selected row -- centred on it -- so selecting a row you could see moved it
// out from under your finger. It stays where it is now, and the window only
// moves when the selection would otherwise leave it.
TEST (ListScroll, SelectingAVisibleRowDoesNotMoveTheList)
{
  constexpr int rows = 10, total = 85;

  for (int top : { 0, 12, 40, total - rows })
    for (int selected = top; selected < top + rows; ++selected)
      EXPECT_EQ (scrollToShow (top, selected, rows, total), top)
          << "top " << top << ", selected " << selected;
}

// ... and when it would, it moves as little as it can: the row appears at the
// edge it was reached from, which is where the eye already is.
TEST (ListScroll, ReachingPastTheEdgeMovesTheListByOneRow)
{
  constexpr int rows = 10, total = 85;

  EXPECT_EQ (scrollToShow (20, 30, rows, total), 21);
  EXPECT_EQ (scrollToShow (20, 19, rows, total), 19);
}

TEST (ListScroll, TheListNeverScrollsPastEitherEnd)
{
  constexpr int rows = 10, total = 85;

  EXPECT_EQ (scrollBy (0, -5, rows, total), 0);
  EXPECT_EQ (scrollBy (total - rows, 5, rows, total), total - rows);

  // A list shorter than the window does not scroll at all.
  EXPECT_EQ (scrollBy (0, 4, 10, 6), 0);
  EXPECT_EQ (scrollToShow (0, 3, 10, 6), 0);
}

// Dragging down brings what is above into view, the way a finger on a phone
// moves the page rather than a cursor. It went the other way, which is the
// one convention every hand in the room already has.
TEST (ListScroll, DraggingDownBringsEarlierRowsIntoView)
{
  constexpr int rows = 10, total = 85;

  // A drag downwards is a negative increment -- see TouchControl, which
  // counts upwards as more.
  EXPECT_LT (scrollBy (40, -3, rows, total), 40);
  EXPECT_GT (scrollBy (40, 3, rows, total), 40);
}
