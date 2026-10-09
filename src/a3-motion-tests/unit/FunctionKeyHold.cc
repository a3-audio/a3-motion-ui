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

#include <a3-motion-ui/io/FunctionKeyHold.hh>

using namespace a3;

// The panel and the PADS page are two places to press one key, the way the
// panel's two end columns are. A key is down while either is down, and what
// the key means is handled once per real change, not once per place.

TEST (FunctionKeyHold, AScreenPressIsAPress)
{
  EndKeyHold hold;

  EXPECT_EQ (hold.set (EndKey::Shift, KeySource::Screen, true), true);
  EXPECT_TRUE (hold.isDown (EndKey::Shift));

  EXPECT_EQ (hold.set (EndKey::Shift, KeySource::Screen, false), false);
  EXPECT_FALSE (hold.isDown (EndKey::Shift));
}

TEST (FunctionKeyHold, APanelPressIsAPress)
{
  EndKeyHold hold;

  EXPECT_EQ (hold.set (EndKey::PlayAll, KeySource::Panel, true), true);
  EXPECT_TRUE (hold.isDown (EndKey::PlayAll));
}

// Holding SHIFT on the panel and touching it on the screen too must not read
// as a second press -- and letting go of one of them must not read as a
// release while the other is still held.
TEST (FunctionKeyHold, TheKeyIsDownWhileEitherPlaceHoldsIt)
{
  EndKeyHold hold;

  ASSERT_EQ (hold.set (EndKey::Shift, KeySource::Panel, true), true);
  EXPECT_EQ (hold.set (EndKey::Shift, KeySource::Screen, true), std::nullopt);
  EXPECT_EQ (hold.set (EndKey::Shift, KeySource::Panel, false), std::nullopt);
  EXPECT_TRUE (hold.isDown (EndKey::Shift));

  EXPECT_EQ (hold.set (EndKey::Shift, KeySource::Screen, false), false);
  EXPECT_FALSE (hold.isDown (EndKey::Shift));
}

// The same rule joins the panel's two end columns: TAP on the left and TAP on
// the right are one key.
TEST (FunctionKeyHold, TheTwoSidesOfThePanelAreOneKey)
{
  EndColumnHold hold;

  ASSERT_EQ (hold.set (EndKey::Tap, PanelSide::Left, true), true);
  EXPECT_EQ (hold.set (EndKey::Tap, PanelSide::Right, true), std::nullopt);
  EXPECT_EQ (hold.set (EndKey::Tap, PanelSide::Left, false), std::nullopt);
  EXPECT_EQ (hold.set (EndKey::Tap, PanelSide::Right, false), false);
}

// A repeated report of the same state is no change: a juce::Value can be told
// the same thing twice.
TEST (FunctionKeyHold, TheSameStateTwiceIsNoChange)
{
  EndKeyHold hold;

  EXPECT_EQ (hold.set (EndKey::Tap, KeySource::Screen, false), std::nullopt);
  ASSERT_EQ (hold.set (EndKey::Tap, KeySource::Screen, true), true);
  EXPECT_EQ (hold.set (EndKey::Tap, KeySource::Screen, true), std::nullopt);
}

// Nine keys, nine states: holding one says nothing about another -- A1 on the
// left and A2 beside it on the right are two keys.
TEST (FunctionKeyHold, EachKeyIsHeldOnItsOwn)
{
  EndKeyHold hold;
  hold.set (EndKey::Action1, KeySource::Screen, true);

  for (auto const key : allEndKeys)
    EXPECT_EQ (hold.isDown (key), key == EndKey::Action1);
}
