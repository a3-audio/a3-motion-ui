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
// panel's two end columns already are. A key is down while either is down, and
// what the key means is handled once per real change, not once per place.

TEST (FunctionKeyHold, AScreenPressIsAPress)
{
  FunctionKeyHold hold;

  EXPECT_EQ (hold.set (FunctionKey::Shift, KeySource::Screen, true), true);
  EXPECT_TRUE (hold.isDown (FunctionKey::Shift));

  EXPECT_EQ (hold.set (FunctionKey::Shift, KeySource::Screen, false), false);
  EXPECT_FALSE (hold.isDown (FunctionKey::Shift));
}

TEST (FunctionKeyHold, APanelPressIsAPress)
{
  FunctionKeyHold hold;

  EXPECT_EQ (hold.set (FunctionKey::Record, KeySource::Panel, true), true);
  EXPECT_TRUE (hold.isDown (FunctionKey::Record));
}

// Holding SHIFT on the panel and touching it on the screen too must not read
// as a second press -- and letting go of one of them must not read as a
// release while the other is still held.
TEST (FunctionKeyHold, TheKeyIsDownWhileEitherPlaceHoldsIt)
{
  FunctionKeyHold hold;

  ASSERT_EQ (hold.set (FunctionKey::Shift, KeySource::Panel, true), true);
  EXPECT_EQ (hold.set (FunctionKey::Shift, KeySource::Screen, true),
             std::nullopt);
  EXPECT_EQ (hold.set (FunctionKey::Shift, KeySource::Panel, false),
             std::nullopt);
  EXPECT_TRUE (hold.isDown (FunctionKey::Shift));

  EXPECT_EQ (hold.set (FunctionKey::Shift, KeySource::Screen, false), false);
  EXPECT_FALSE (hold.isDown (FunctionKey::Shift));
}

// A repeated report of the same state is no change: a juce::Value can be told
// the same thing twice.
TEST (FunctionKeyHold, TheSameStateTwiceIsNoChange)
{
  FunctionKeyHold hold;

  EXPECT_EQ (hold.set (FunctionKey::Tap, KeySource::Screen, false),
             std::nullopt);
  ASSERT_EQ (hold.set (FunctionKey::Tap, KeySource::Screen, true), true);
  EXPECT_EQ (hold.set (FunctionKey::Tap, KeySource::Screen, true),
             std::nullopt);
}

// Six keys, six states: holding one says nothing about another.
TEST (FunctionKeyHold, EachKeyIsHeldOnItsOwn)
{
  FunctionKeyHold hold;
  hold.set (FunctionKey::Shift, KeySource::Screen, true);

  for (auto const key : functionKeyOrder)
    EXPECT_EQ (hold.isDown (key), key == FunctionKey::Shift);
}
