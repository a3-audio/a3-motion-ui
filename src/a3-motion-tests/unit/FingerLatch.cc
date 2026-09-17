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

#include <a3-motion-ui/components/FingerLatch.hh>

using namespace a3;

// "scroll mit einem oder zwei fingern". Two fingers on a list land on two hit
// areas, or twice on one, and each would scroll it: the list ran at double
// speed, or jumped when the second finger restarted the drag the first was
// making. The first finger leads; the second is ignored until the first lets
// go.
TEST (FingerLatch, TheFirstFingerLeads)
{
  FingerLatch latch;
  EXPECT_TRUE (latch.claim (3));
  EXPECT_TRUE (latch.leads (3));

  EXPECT_FALSE (latch.claim (5)) << "a second finger does not take over";
  EXPECT_FALSE (latch.leads (5));
  EXPECT_TRUE (latch.leads (3));
}

TEST (FingerLatch, ReleasingTheLeaderFreesTheList)
{
  FingerLatch latch;
  latch.claim (3);
  latch.release (5); // the other finger coming up changes nothing
  EXPECT_TRUE (latch.leads (3));

  latch.release (3);
  EXPECT_TRUE (latch.claim (5));
  EXPECT_TRUE (latch.leads (5));
}

TEST (FingerLatch, TheSameFingerCanClaimAgain)
{
  FingerLatch latch;
  EXPECT_TRUE (latch.claim (2));
  EXPECT_TRUE (latch.claim (2));
}

TEST (FingerLatch, AListHasOneLatchWhereverItsAreasAre)
{
  // The skin editor's rows, its scroll area and the strips beside it are one
  // list: a finger on a strip and one on the rows are two fingers on it.
  EXPECT_EQ (&FingerLatch::forGroup (FingerLatch::menuList),
             &FingerLatch::forGroup (FingerLatch::menuList));
}
