/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-ui/io/PadFunctions.hh>

#include <set>

using namespace a3;

// One clip per channel (2026-09-27): a channel's eight pads are Play/Pause,
// Page and six action buttons, laid out as the panel stands -- Play where
// slot 1's Play was, Page where Stop was, the six in reading order below.
TEST (PadFunctions, AChannelIsPlayPageAndSixActions)
{
  EXPECT_EQ (padFunctionByPadIndex[0], PadFunction::PlayPause);
  EXPECT_EQ (padFunctionByPadIndex[4], PadFunction::Page);

  auto actions = 0;
  for (auto const function : padFunctionByPadIndex)
    if (function == PadFunction::Action)
      ++actions;
  EXPECT_EQ (actions, numActionButtons);
}

// Left to right, top to bottom: A1 A2 / A3 A4 / A5 A6 across the two columns.
TEST (PadFunctions, TheSixReadInOrderAcrossTheColumns)
{
  EXPECT_EQ (actionButtonForPad[1], 0);
  EXPECT_EQ (actionButtonForPad[5], 1);
  EXPECT_EQ (actionButtonForPad[2], 2);
  EXPECT_EQ (actionButtonForPad[6], 3);
  EXPECT_EQ (actionButtonForPad[3], 4);
  EXPECT_EQ (actionButtonForPad[7], 5);
  EXPECT_EQ (actionButtonForPad[0], -1) << "Play is no action";
  EXPECT_EQ (actionButtonForPad[4], -1) << "Page is no action";
}

// The way back from a button to its pad, for every button.
TEST (PadFunctions, EveryButtonHasItsPad)
{
  std::set<index_t> pads;
  for (int button = 0; button < numActionButtons; ++button)
    {
      auto const pad = padIndexForAction (button);
      EXPECT_EQ (actionButtonForPad[pad], button);
      pads.insert (pad);
    }
  EXPECT_EQ (pads.size (), static_cast<size_t> (numActionButtons));
  EXPECT_EQ (padIndexFor (PadFunction::PlayPause), 0u);
  EXPECT_EQ (padIndexFor (PadFunction::Page), 4u);
}

TEST (PadFunctions, OneClipPerChannel)
{
  EXPECT_EQ (numPadSlots, 1u);
}
