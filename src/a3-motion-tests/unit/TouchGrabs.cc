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

// ── ten fingers ─────────────────────────────────────────────────────────
//
// The panel reports ten contacts and JUCE allows touch indices up to 100, so
// nothing outside caps this. What caps it is that there are four channels:
// four fingers can each hold a blob, the rest find nothing free. They still
// have to be tracked, or lifting one of them would look like the last finger
// leaving.

TEST (TouchGrabs, TenFingersAreAllTracked)
{
  TouchGrabs grabs;
  for (int source = 0; source < 10; ++source)
    grabs.down (source, source < 4 ? std::optional<index_t>{ (index_t)source }
                                   : std::optional<index_t>{});

  EXPECT_EQ (grabs.heldChannels ().size (), 4u);
  for (index_t channel = 0; channel < 4; ++channel)
    EXPECT_TRUE (grabs.isHeld (channel)) << "channel " << channel;

  for (int source = 4; source < 10; ++source)
    EXPECT_FALSE (grabs.channelFor (source).has_value ())
        << "source " << source << " had nothing free to take";
}

TEST (TouchGrabs, LiftingOneOfTenLeavesTheRestAlone)
{
  TouchGrabs grabs;
  for (int source = 0; source < 10; ++source)
    grabs.down (source, source < 4 ? std::optional<index_t>{ (index_t)source }
                                   : std::optional<index_t>{});

  EXPECT_EQ (grabs.up (2), std::optional<index_t>{ 2u });

  EXPECT_FALSE (grabs.isHeld (2u)) << "freed for the next finger";
  EXPECT_EQ (grabs.heldChannels ().size (), 3u);
  EXPECT_FALSE (grabs.empty ()) << "nine fingers still down";
}

TEST (TouchGrabs, TheScreenIsOnlyEmptyAfterTheLastOfTenLifts)
{
  TouchGrabs grabs;
  for (int source = 0; source < 10; ++source)
    grabs.down (source, {});

  for (int source = 0; source < 9; ++source)
    {
      grabs.up (source);
      EXPECT_FALSE (grabs.empty ()) << "after lifting " << source;
    }

  grabs.up (9);
  EXPECT_TRUE (grabs.empty ());
}

// A recording follows one finger, and it has to be the same one throughout.
// With a single pointer that was automatic; with ten, every one of them would
// otherwise write the recording position and the trajectory would jump
// between them.
TEST (TouchGrabs, TheRecordingFollowsTheFingerThatStartedIt)
{
  TouchGrabs grabs;
  grabs.down (3, {});
  grabs.down (7, {});
  grabs.down (1, {});

  EXPECT_EQ (grabs.firstSource (), std::optional<int>{ 3 });
}

TEST (TouchGrabs, WhenThatFingerLiftsTheNextOneTakesOver)
{
  TouchGrabs grabs;
  grabs.down (3, {});
  grabs.down (7, {});

  grabs.up (3);

  EXPECT_EQ (grabs.firstSource (), std::optional<int>{ 7 })
      << "the oldest finger still down, not the lowest index";
}

TEST (TouchGrabs, NoFingerDownMeansNoFirstSource)
{
  TouchGrabs grabs;
  EXPECT_FALSE (grabs.firstSource ().has_value ());

  grabs.down (0, {});
  grabs.up (0);
  EXPECT_FALSE (grabs.firstSource ().has_value ());
}
