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

#include <a3-motion-ui/components/AppView.hh>

using namespace a3;

TEST (AppView, NamesAreTheWordsOnTheKey)
{
  EXPECT_STREQ (appViewName (AppView::Full), "FULL");
  EXPECT_STREQ (appViewName (AppView::Fpv), "FPV");
}

TEST (AppView, ToggledGoesBackAndForth)
{
  EXPECT_EQ (toggled (AppView::Full), AppView::Fpv);
  EXPECT_EQ (toggled (AppView::Fpv), AppView::Full);
}
