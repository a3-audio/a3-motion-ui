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

#include <a3-motion-ui/components/PatternProgressBar.hh>

using namespace a3;

// The bar answers "how far am I", which the sphere cannot: on the sphere the
// passes lie on top of each other and a path crossing itself is no longer two
// things. On the bar time is the axis, so every moment sits somewhere once.

TEST (PatternProgressBar, LongPatternsAreDividedIntoBars)
{
  EXPECT_EQ (progressBarDivisions (64.f, 4), 16);
  EXPECT_EQ (progressBarDivisions (16.f, 4), 4);
}

TEST (PatternProgressBar, ShortPatternsAreDividedIntoBeats)
{
  EXPECT_EQ (progressBarDivisions (4.f, 4), 4) << "one bar, so beats";
  EXPECT_EQ (progressBarDivisions (2.f, 4), 2);
}

// Sixty-four marks in a strip a few pixels tall is not a grid, it is a smear.
TEST (PatternProgressBar, TheGridStaysCountable)
{
  EXPECT_LE (progressBarDivisions (64.f, 4), 16);
  EXPECT_LE (progressBarDivisions (128.f, 4), 32);
}

TEST (PatternProgressBar, TheGridNeverDisappears)
{
  EXPECT_GE (progressBarDivisions (0.25f, 4), 1);
  EXPECT_GE (progressBarDivisions (0.f, 4), 1);
}

TEST (PatternProgressBar, AnOddMetreStillDividesByBars)
{
  EXPECT_EQ (progressBarDivisions (21.f, 7), 3) << "three bars of seven";
}
