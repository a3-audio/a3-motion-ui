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

#include <a3-motion-ui/io/LedCache.hh>

using namespace a3;

namespace
{
auto const red = juce::Colours::red;
auto const blue = juce::Colours::blue;
auto const cyan = juce::Colour::fromRGB (31, 214, 205);
}

// The whole reason it exists: the serial link is shared with the input frames.
TEST (LedCache, AColourThatChangesNothingIsNotWrittenAgain)
{
  LedCache cache;

  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Menu, blue));
  EXPECT_FALSE (cache.shouldWrite (FunctionKey::Menu, blue));
  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Menu, red));
  EXPECT_FALSE (cache.shouldWrite (FunctionKey::Menu, red));
}

// Six keys, six answers. A key is not spoken for by its neighbour.
TEST (LedCache, EachKeyIsRememberedOnItsOwn)
{
  LedCache cache;

  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Menu, blue));
  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Record, blue));
  EXPECT_FALSE (cache.shouldWrite (FunctionKey::Menu, blue));
  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Tap, red));
}

// This is the bug, in one test. Something wrote past the record -- a probe on
// the serial link, or the resting light changing under a key whose own colour
// did not -- and the key must be able to be put back. Before this, the record
// said "already blue" and the correction was dropped, and MENU sat dark on a
// panel where every other key was right.
TEST (LedCache, ForgettingWhatIsShownLetsEveryKeyBeWrittenAgain)
{
  LedCache cache;

  ASSERT_TRUE (cache.shouldWrite (FunctionKey::Menu, blue));
  ASSERT_TRUE (cache.shouldWrite (FunctionKey::Record, red));
  ASSERT_FALSE (cache.shouldWrite (FunctionKey::Menu, blue));

  cache.forgetWhatIsShown ();

  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Menu, blue));
  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Record, red));
}

// What to write after forgetting has to come from somewhere, or the keys go
// back to a guess rather than to what they were meant to show. The wanted
// colour is the *logical* one -- what the key was asked for -- and it survives,
// which is the whole difference between the two halves of this record.
TEST (LedCache, WhatAKeyWasAskedForSurvivesForgetting)
{
  LedCache cache;

  cache.remember (FunctionKey::Menu, blue);
  cache.remember (FunctionKey::Shift, juce::Colour{});
  ASSERT_TRUE (cache.shouldWrite (FunctionKey::Menu, blue));
  ASSERT_TRUE (cache.shouldWrite (FunctionKey::Shift, cyan));

  cache.forgetWhatIsShown ();

  ASSERT_TRUE (cache.wantedFor (FunctionKey::Menu).has_value ());
  EXPECT_EQ (*cache.wantedFor (FunctionKey::Menu), blue);

  // Transparent is a colour here, not an absence: it means "nothing to
  // report", and outputButtonLED resolves it to the resting light. Losing it
  // would lose the key.
  ASSERT_TRUE (cache.wantedFor (FunctionKey::Shift).has_value ());
  EXPECT_TRUE (cache.wantedFor (FunctionKey::Shift)->isTransparent ());
}

// A key nobody has asked for yet has nothing to be put back to, and says so
// rather than answering black.
TEST (LedCache, AKeyNeverAskedForHasNoWantedColour)
{
  LedCache cache;

  EXPECT_FALSE (cache.wantedFor (FunctionKey::ClockMode).has_value ());

  cache.remember (FunctionKey::ClockMode, red);
  EXPECT_TRUE (cache.wantedFor (FunctionKey::ClockMode).has_value ());
}

// The wanted colour and the shown colour are two different things, and the
// resting light is exactly where they come apart: Menu's own colour never
// changes, so only the resolved one can say the key needs writing.
TEST (LedCache, WantedAndShownAreNotTheSameThing)
{
  LedCache cache;

  cache.remember (FunctionKey::Shift, juce::Colour{});
  ASSERT_TRUE (cache.shouldWrite (FunctionKey::Shift, cyan));

  // Same wanted colour, a different resting light behind it.
  cache.remember (FunctionKey::Shift, juce::Colour{});
  EXPECT_TRUE (cache.shouldWrite (FunctionKey::Shift, red))
      << "a changed resting light must reach the key";
}
