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

#include <a3-motion-ui/components/ColumnBreak.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

using namespace a3;

namespace
{
// Wide enough that four columns each clear minimumChannelWidth with room to
// spare. Not a measurement of any real screen -- the rule is about the
// relation between an area and a threshold, and a test that quoted the
// device's pixels would have to be rewritten for the next device.
constexpr int roomyWidth = minimumChannelWidth * 6;
constexpr int roomyHeight = minimumMotionHeight * 4;
}

// The ordinary case, and the one the device is built for.
TEST (ColumnBreak, FourFitSideBySideWhileEachClearsTheThreshold)
{
  auto const area = juce::Rectangle<int> (0, 0, roomyWidth, roomyHeight);
  auto const broken = breakColumns (area, 4, minimumChannelWidth,
                                    minimumMotionHeight);

  EXPECT_TRUE (broken.fits);
  EXPECT_EQ (broken.columns, 4);
  EXPECT_EQ (broken.rows, 1);
}

// Exactly at the threshold still counts. A rule that broke one pixel early
// would reflow the device it was written for.
TEST (ColumnBreak, TheThresholdItselfIsWideEnough)
{
  auto const area = juce::Rectangle<int> (0, 0, minimumChannelWidth * 4,
                                          roomyHeight);
  auto const broken = breakColumns (area, 4, minimumChannelWidth,
                                    minimumMotionHeight);

  EXPECT_TRUE (broken.fits);
  EXPECT_EQ (broken.columns, 4);
}

// One pixel narrower and four columns would each be under the threshold. The
// answer is two by two, not four thin ones: a target under a fingertip is not
// a compromise but a fault.
TEST (ColumnBreak, TooNarrowForFourBreaksIntoTwoByTwo)
{
  auto const area = juce::Rectangle<int> (0, 0, minimumChannelWidth * 4 - 1,
                                          roomyHeight);
  auto const broken = breakColumns (area, 4, minimumChannelWidth,
                                    minimumMotionHeight);

  EXPECT_TRUE (broken.fits);
  EXPECT_EQ (broken.columns, 2);
  EXPECT_EQ (broken.rows, 2);
}

// Narrower still: one column, four rows.
TEST (ColumnBreak, TooNarrowForTwoBreaksIntoOneColumn)
{
  auto const area = juce::Rectangle<int> (0, 0, minimumChannelWidth * 2 - 1,
                                          roomyHeight);
  auto const broken = breakColumns (area, 4, minimumChannelWidth,
                                    minimumMotionHeight);

  EXPECT_EQ (broken.columns, 1);
  EXPECT_EQ (broken.rows, 4);
}

// Breaking costs height, and at some point there is none left. Then the rule
// says so rather than handing back rectangles nobody can hit -- the caller
// draws a short message instead of a mixer that cannot be used.
TEST (ColumnBreak, ItSaysWhenNothingFits)
{
  auto const area = juce::Rectangle<int> (0, 0, minimumChannelWidth * 2 - 1,
                                          minimumMotionHeight - 1);
  auto const broken = breakColumns (area, 4, minimumChannelWidth,
                                    minimumMotionHeight);

  EXPECT_FALSE (broken.fits);
}

// The cells tile the area: none empty, none overlapping, none outside.
//
// Twice, because a width that divides by the column count exercises none of
// cellIn's arithmetic: the cells are measured from the left and the top and
// the remainder is left standing against the right and bottom edges, which is
// what keeps a row lined up with the grid above it. A width divisible by four
// leaves no remainder to put anywhere.
TEST (ColumnBreak, TheCellsTileTheAreaWithoutOverlapping)
{
  for (auto const spare : { 0, 3 })
    {
      auto const area
          = juce::Rectangle<int> (7, 11, roomyWidth + spare, roomyHeight);
      auto const broken = breakColumns (area, 4, minimumChannelWidth,
                                        minimumMotionHeight);
      ASSERT_TRUE (broken.fits) << spare;

      for (auto i = 0; i < 4; ++i)
        {
          auto const cell = cellIn (area, broken, i);
          EXPECT_FALSE (cell.isEmpty ()) << i << " with " << spare << " spare";
          EXPECT_TRUE (area.contains (cell))
              << i << " with " << spare << " spare";

          for (auto j = i + 1; j < 4; ++j)
            EXPECT_FALSE (cell.intersects (cellIn (area, broken, j)))
                << i << " overlaps " << j << " with " << spare << " spare";
        }

      // The first cell starts at the area's own corner and the leftover is
      // all of it, at the far edge.
      EXPECT_EQ (cellIn (area, broken, 0).getTopLeft (), area.getTopLeft ());
      EXPECT_EQ (area.getRight () - cellIn (area, broken, 3).getRight (),
                 area.getWidth () % broken.columns);
    }
}

// Reading order, so channel 1 is where a hand looks for it in every
// arrangement: left to right, then down.
TEST (ColumnBreak, CellsAreInReadingOrder)
{
  auto const area = juce::Rectangle<int> (0, 0, minimumChannelWidth * 4 - 1,
                                          minimumMotionHeight * 4);
  auto const broken = breakColumns (area, 4, minimumChannelWidth,
                                    minimumMotionHeight);
  ASSERT_EQ (broken.columns, 2);

  EXPECT_LT (cellIn (area, broken, 0).getX (),
             cellIn (area, broken, 1).getX ());
  EXPECT_LT (cellIn (area, broken, 0).getY (),
             cellIn (area, broken, 2).getY ());
}

// An empty area is not a crash. resized() is called with one before the
// window has a size.
TEST (ColumnBreak, AnEmptyAreaFitsNothingAndDoesNotDivideByZero)
{
  auto const broken = breakColumns ({}, 4, minimumChannelWidth,
                                    minimumMotionHeight);
  EXPECT_FALSE (broken.fits);
  EXPECT_TRUE (cellIn ({}, broken, 0).isEmpty ());
}
