/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-ui/io/LedState.hh>

using namespace a3;

namespace
{
auto const red = juce::Colours::red;
auto const green = juce::Colours::green;
auto const blue = juce::Colours::blue;

// 9 end keys and 4 x 8 pads: the panel's whole picture at start.
constexpr std::size_t panelLeds = 41;
}

// The fault it exists for: the output FIFO held 31 messages, the panel is 41
// LEDs, and painting it at start dropped whatever did not fit.
TEST (LedState, AWholePanelWrittenBeforeOneTakeLosesNothing)
{
  LedState state (panelLeds);

  for (std::size_t slot = 0; slot < panelLeds; ++slot)
    state.set (slot, red);

  auto const changed = state.takeChanged ();

  ASSERT_EQ (changed.size (), panelLeds);
  for (std::size_t slot = 0; slot < panelLeds; ++slot)
    {
      EXPECT_EQ (changed[slot].slot, slot);
      EXPECT_EQ (changed[slot].colour, red);
    }
}

TEST (LedState, SeveralChangesToOneLedGoOutOnceAsTheLast)
{
  LedState state (panelLeds);

  state.set (12, red);
  state.set (12, green);
  state.set (12, blue);

  auto const changed = state.takeChanged ();

  ASSERT_EQ (changed.size (), 1u);
  EXPECT_EQ (changed[0].slot, 12u);
  EXPECT_EQ (changed[0].colour, blue);
}

TEST (LedState, WhatWasTakenIsNotTakenAgain)
{
  LedState state (panelLeds);
  state.set (3, red);
  state.takeChanged ();

  EXPECT_TRUE (state.takeChanged ().empty ());
}

TEST (LedState, ASetAfterATakeGoesOutAgain)
{
  LedState state (panelLeds);
  state.set (3, red);
  state.takeChanged ();

  state.set (3, red);
  auto const changed = state.takeChanged ();

  ASSERT_EQ (changed.size (), 1u);
  EXPECT_EQ (changed[0].colour, red);
}

TEST (LedState, ASlotPastTheEndIsIgnored)
{
  LedState state (panelLeds);

  state.set (panelLeds, red);

  EXPECT_TRUE (state.takeChanged ().empty ());
}

TEST (LedState, AResentSlotGoesOutAgainWithItsLatestColour)
{
  LedState state (panelLeds);
  state.set (7, red);
  state.set (7, green);
  state.takeChanged ();

  state.resend (7);
  state.resend (panelLeds);   // past the end: ignored
  auto const changed = state.takeChanged ();

  ASSERT_EQ (changed.size (), 1u);
  EXPECT_EQ (changed[0].slot, 7u);
  EXPECT_EQ (changed[0].colour, green);
}
