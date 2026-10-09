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

#include <a3-motion-ui/components/SceneLaunch.hh>

using namespace a3;

namespace
{
using S = Pattern::Status;
using Picks = std::vector<bool>;
}

// PLAY all toggles the room as one: anything running and it pauses what runs;
// nothing running and it starts everything that stands still. A toggle per
// channel would start half the room and stop the other half.
TEST (SceneLaunch, AnythingPlayingPausesWhatPlays)
{
  PlayAllPress playAll;
  EXPECT_EQ (playAll.press ({ S::Playing, S::Idle, S::Empty, S::Playing }, 1000),
             (Picks{ true, false, false, true }));
}

TEST (SceneLaunch, NothingPlayingStartsEverythingThatStandsStill)
{
  PlayAllPress playAll;
  EXPECT_EQ (playAll.press ({ S::Idle, S::Empty, S::Idle, S::Recording }, 1000),
             (Picks{ true, false, true, false }));
}

// A start waiting for its downbeat is running for this purpose: PLAY all then
// calls it off, as the channel's own pad would.
TEST (SceneLaunch, AWaitingStartCountsAsPlaying)
{
  PlayAllPress playAll;
  EXPECT_EQ (playAll.press ({ S::ScheduledForPlaying, S::Idle }, 1000),
             (Picks{ true, false }));
}

// A take is left to REC: PLAY all neither counts it nor touches it.
TEST (SceneLaunch, ARecordingChannelIsLeftAlone)
{
  PlayAllPress playAll;
  EXPECT_EQ (playAll.press ({ S::Recording, S::Playing }, 1000),
             (Picks{ false, true }));
}

// Two taps go back to the top, as on a channel's Play|Pause -- for the
// channels the first tap reached and no others. The room toggled on the first
// tap, so the rule asked again would pick a different set.
TEST (SceneLaunch, TheSecondTapOfADoubleTapReachesTheSameChannels)
{
  PlayAllPress playAll;
  ASSERT_EQ (playAll.press ({ S::Playing, S::Idle, S::Playing }, 1000),
             (Picks{ true, false, true }));

  EXPECT_EQ (playAll.press ({ S::Idle, S::Idle, S::Idle }, 1000 + 200),
             (Picks{ true, false, true }));

  // Used up: the next tap is the rule again.
  EXPECT_EQ (playAll.press ({ S::Idle, S::Idle, S::Idle }, 1000 + 400),
             (Picks{ true, true, true }));
}

TEST (SceneLaunch, TapsFurtherApartAreEachTheRule)
{
  PlayAllPress playAll;
  playAll.press ({ S::Playing, S::Idle }, 1000);
  EXPECT_EQ (playAll.press ({ S::Idle, S::Idle }, 1000 + doubleTapWindowMs + 1),
             (Picks{ true, true }));
}

// One touch arrives twice under X; the twin reaches the same channels, where
// each Play|Pause sees it as a twin and does nothing.
TEST (SceneLaunch, ATwinReachesTheSameChannelsAndUsesNothingUp)
{
  PlayAllPress playAll;
  playAll.press ({ S::Playing, S::Idle }, 1000);
  EXPECT_EQ (playAll.press ({ S::Idle, S::Idle }, 1000 + 5),
             (Picks{ true, false }));
  EXPECT_EQ (playAll.press ({ S::Idle, S::Idle }, 1000 + 200),
             (Picks{ true, false }))
      << "still the double tap of the first press";
}
