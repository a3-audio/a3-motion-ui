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

#include <a3-motion-ui/components/ActionChain.hh>

using namespace a3;

// What a button does when its accent is over (2026-09-28): nothing, as
// before, or fire another button of the same channel -- "then A3". The chain
// is decided here, on the message thread, from the engine's count of accents
// that have ended; no script is ever worked out on the clock's thread.

namespace
{
AfterTable
noAfter ()
{
  return {};
}
}

TEST (ActionChain, WithNothingAfterAnEndFiresNothing)
{
  ActionChain chain;
  chain.pressed (0, 0);
  EXPECT_FALSE (chain.accentEnded (1, noAfter ()).has_value ());
}

TEST (ActionChain, AnEndFiresTheButtonAfterIt)
{
  auto after = noAfter ();
  after[0] = 2;
  ActionChain chain;
  chain.pressed (0, 5);

  auto const next = chain.accentEnded (6, after);
  ASSERT_TRUE (next.has_value ());
  EXPECT_EQ (*next, 2);
}

// Once per end: asked again about the same end, it says nothing more.
TEST (ActionChain, OncePerEnd)
{
  auto after = noAfter ();
  after[0] = 2;
  ActionChain chain;
  chain.pressed (0, 0);

  EXPECT_TRUE (chain.accentEnded (1, after).has_value ());
  EXPECT_FALSE (chain.accentEnded (1, after).has_value ());
}

// An end the chain saw before the press is not this press's end.
TEST (ActionChain, AnEndBeforeThePressIsNotItsEnd)
{
  auto after = noAfter ();
  after[0] = 2;
  ActionChain chain;
  chain.pressed (0, 4);
  EXPECT_FALSE (chain.accentEnded (4, after).has_value ());
}

// A1 then A2 then A1: a loop, and a musical one.
TEST (ActionChain, AChainMayLoop)
{
  auto after = noAfter ();
  after[0] = 1;
  after[1] = 0;
  ActionChain chain;
  chain.pressed (0, 0);

  auto first = chain.accentEnded (1, after);
  ASSERT_TRUE (first.has_value ());
  EXPECT_EQ (*first, 1);
  chain.pressed (*first, 1);

  auto second = chain.accentEnded (2, after);
  ASSERT_TRUE (second.has_value ());
  EXPECT_EQ (*second, 0);
}

// Another button pressed while one runs takes over: its own "after" counts,
// not the one it replaced.
TEST (ActionChain, APressElsewhereBreaksIt)
{
  auto after = noAfter ();
  after[0] = 1;
  ActionChain chain;
  chain.pressed (0, 0);
  chain.pressed (3, 0);
  EXPECT_FALSE (chain.accentEnded (1, after).has_value ());
}

// Play|Pause or Stop on the channel: the chain is over.
TEST (ActionChain, PlayOrStopBreaksIt)
{
  auto after = noAfter ();
  after[0] = 1;
  ActionChain chain;
  chain.pressed (0, 0);
  chain.broken ();
  EXPECT_FALSE (chain.accentEnded (1, after).has_value ());
}

// A button may follow itself -- a repeat -- but only once per end, never
// again within the same one.
TEST (ActionChain, AButtonMayFollowItselfOncePerEnd)
{
  auto after = noAfter ();
  after[2] = 2;
  ActionChain chain;
  chain.pressed (2, 0);

  auto const again = chain.accentEnded (1, after);
  ASSERT_TRUE (again.has_value ());
  EXPECT_EQ (*again, 2);
  chain.pressed (2, 1);
  EXPECT_FALSE (chain.accentEnded (1, after).has_value ())
      << "re-entered within the same end";
  EXPECT_TRUE (chain.accentEnded (2, after).has_value ());
}

// The key steps --, A1 .. A6 and round; two taps put it back to --.
TEST (ActionChain, TheAfterKeyStepsRound)
{
  std::optional<int> after;
  after = stepAfter (after, 1);
  ASSERT_TRUE (after.has_value ());
  EXPECT_EQ (*after, 0);
  for (int i = 0; i < 5; ++i)
    after = stepAfter (after, 1);
  ASSERT_TRUE (after.has_value ());
  EXPECT_EQ (*after, 5);
  EXPECT_FALSE (stepAfter (after, 1).has_value ());
  EXPECT_EQ (stepAfter (std::nullopt, -1), std::optional<int> (5));
}

TEST (ActionChain, TheAfterKeySaysWhereItGoes)
{
  EXPECT_EQ (afterName (std::nullopt), juce::String ("--"));
  EXPECT_EQ (afterName (2), juce::String ("A3"));
  EXPECT_EQ (afterFromName ("A3"), std::optional<int> (2));
  EXPECT_FALSE (afterFromName ("A7").has_value ());
  EXPECT_FALSE (afterFromName ("").has_value ());
}

// A push on the panel or the PADS page shows the ACTION page with the pushed
// button chosen; with Shift -- a preview, auditioned while somewhere else,
// FILES most of all -- it leaves the page where it is. The bar's own ACT key,
// a scene and a chain never move it.
TEST (ActionChain, WhichPushesShowTheActionPage)
{
  EXPECT_TRUE (actionPressShowsItsPage (PadSource::Panel, false));
  EXPECT_TRUE (actionPressShowsItsPage (PadSource::PadsPage, false));
  EXPECT_FALSE (actionPressShowsItsPage (PadSource::Panel, true));
  EXPECT_FALSE (actionPressShowsItsPage (PadSource::PadsPage, true));
  EXPECT_FALSE (actionPressShowsItsPage (PadSource::TransportKey, false));
  EXPECT_FALSE (actionPressShowsItsPage (PadSource::Scene, false));
  EXPECT_FALSE (actionPressShowsItsPage (PadSource::Chain, false));
}
