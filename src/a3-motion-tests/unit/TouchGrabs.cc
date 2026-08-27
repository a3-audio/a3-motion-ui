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

#include <a3-motion-ui/components/TouchGrabs.hh>

using namespace a3;

// Until JUCE 9 there was one grabbed channel, because Linux delivered every
// finger as the one mouse pointer and a second grab could only mean the first
// had moved. Now each touch arrives as its own MouseInputSource, and what the
// component has to answer is per source: which channel is this finger holding,
// and which channels are somebody else's.

TEST (TouchGrabs, ASourceGivesBackTheChannelItGrabbed)
{
  TouchGrabs grabs;
  grabs.down (0, 2u);

  EXPECT_EQ (grabs.channelFor (0), std::optional<index_t>{ 2u });
}

TEST (TouchGrabs, ASourceThatGrabbedNothingHoldsNothing)
{
  TouchGrabs grabs;
  grabs.down (0, {});

  EXPECT_FALSE (grabs.channelFor (0).has_value ());
  EXPECT_FALSE (grabs.empty ()) << "the finger is still down";
}

TEST (TouchGrabs, AnUnknownSourceHoldsNothing)
{
  TouchGrabs grabs;
  EXPECT_FALSE (grabs.channelFor (7).has_value ());
}

TEST (TouchGrabs, TwoSourcesHoldTheirOwnChannels)
{
  TouchGrabs grabs;
  grabs.down (0, 1u);
  grabs.down (1, 3u);

  EXPECT_EQ (grabs.channelFor (0), std::optional<index_t>{ 1u });
  EXPECT_EQ (grabs.channelFor (1), std::optional<index_t>{ 3u });
}

// The point of the whole exercise: a second finger must take a blob of its
// own, not steal the one already under the first.
TEST (TouchGrabs, AChannelHeldBySomebodyElseIsNotFree)
{
  TouchGrabs grabs;
  grabs.down (0, 1u);

  EXPECT_TRUE (grabs.isHeld (1u));
  EXPECT_FALSE (grabs.isHeld (0u));
  EXPECT_FALSE (grabs.isHeld (3u));
}

TEST (TouchGrabs, LiftingOneSourceLeavesTheOtherHolding)
{
  TouchGrabs grabs;
  grabs.down (0, 1u);
  grabs.down (1, 3u);

  EXPECT_EQ (grabs.up (0), std::optional<index_t>{ 1u });

  EXPECT_FALSE (grabs.channelFor (0).has_value ());
  EXPECT_EQ (grabs.channelFor (1), std::optional<index_t>{ 3u });
  EXPECT_FALSE (grabs.isHeld (1u)) << "released";
  EXPECT_TRUE (grabs.isHeld (3u)) << "still held";
  EXPECT_FALSE (grabs.empty ());
}

// mouseUp used to clear every channel's hold. It has to clear exactly one, or
// lifting one finger hands every other blob back to playback mid-drag.
TEST (TouchGrabs, LiftingGivesBackTheChannelToUnholdAndNoOther)
{
  TouchGrabs grabs;
  grabs.down (4, 2u);

  auto const released = grabs.up (4);

  ASSERT_TRUE (released.has_value ());
  EXPECT_EQ (released.value (), 2u);
  EXPECT_TRUE (grabs.empty ());
}

TEST (TouchGrabs, LiftingASourceThatHeldNothingReleasesNoChannel)
{
  TouchGrabs grabs;
  grabs.down (0, {});

  EXPECT_FALSE (grabs.up (0).has_value ());
  EXPECT_TRUE (grabs.empty ());
}

TEST (TouchGrabs, LiftingAnUnknownSourceIsHarmless)
{
  TouchGrabs grabs;
  grabs.down (0, 1u);

  EXPECT_FALSE (grabs.up (9).has_value ());
  EXPECT_TRUE (grabs.isHeld (1u)) << "the real grab is untouched";
}

TEST (TouchGrabs, EveryHeldChannelIsListedForDisocclusion)
{
  TouchGrabs grabs;
  grabs.down (0, 1u);
  grabs.down (1, {});
  grabs.down (2, 3u);

  auto held = grabs.heldChannels ();
  std::sort (held.begin (), held.end ());

  EXPECT_EQ (held, (std::vector<index_t>{ 1u, 3u }));
}

TEST (TouchGrabs, NothingIsHeldOnAFreshOne)
{
  TouchGrabs grabs;

  EXPECT_TRUE (grabs.empty ());
  EXPECT_TRUE (grabs.heldChannels ().empty ());
  EXPECT_FALSE (grabs.isHeld (0u));
}
