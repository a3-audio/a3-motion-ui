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

#include <set>

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

using namespace a3;

// Every page, listed once. This is the list the tests below walk, and it is
// what forces a new page to be thought about rather than added.
TEST (BarPages, EveryPageAppearsInTheOrderExactlyOnce)
{
  std::set<BarPage> seen;
  for (auto page : barPageOrder)
    EXPECT_TRUE (seen.insert (page).second) << "listed twice";

  EXPECT_EQ (seen.size (), static_cast<std::size_t> (numBarPages));
}

// The two questions every page has to answer are asked below, page by page
// and by name. There used to be a loop over barPageOrder here that called
// both functions and asserted nothing -- it could not fail, which is dead
// weight in the one suite this plan leans on. What it gestured at is covered
// twice over: the two tests below name all six pages and pin both answers for
// each, and a page missing a case in either switch is what -Wswitch-enum is
// for.

// The answers as they stand, so a change to one is a change somebody made on
// purpose. CLIP shows the bar's own three sections; the other four cover them
// with something of their own -- MIX with one channel's strip.
TEST (BarPages, OnlyThePagesWithSomethingOfTheirOwnCoverTheClipArea)
{
  EXPECT_FALSE (pageCoversClipArea (BarPage::Clip));
  EXPECT_FALSE (pageCoversClipArea (BarPage::Record));
  EXPECT_TRUE (pageCoversClipArea (BarPage::Action));
  EXPECT_TRUE (pageCoversClipArea (BarPage::Controller));
  EXPECT_TRUE (pageCoversClipArea (BarPage::Mixer));
  EXPECT_TRUE (pageCoversClipArea (BarPage::Browser));
}

// PADS is the one page that is about every slot at once, so reaching for a
// channel face there is reaching for its clip -- and the face brings the CLIP
// view back with it. Everywhere else the face steps the slot and stays: on
// MIX that is what swaps the strip without leaving the page.
TEST (BarPages, PadsIsTheOnlyPageThatDoesNotDescribeOneClip)
{
  EXPECT_TRUE (pageDescribesAClip (BarPage::Clip));
  EXPECT_TRUE (pageDescribesAClip (BarPage::Record));
  EXPECT_TRUE (pageDescribesAClip (BarPage::Action));
  EXPECT_FALSE (pageDescribesAClip (BarPage::Controller));
  EXPECT_TRUE (pageDescribesAClip (BarPage::Mixer));
  EXPECT_TRUE (pageDescribesAClip (BarPage::Browser));
}
