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

#include <a3-motion-engine/RecMode.hh>

using namespace a3;

namespace
{

// The three modes differ in one thing only: what a pass writes where the
// finger is not. Everything else about recording is the same for all of them,
// so that one decision is worth having on its own, where it can be read as a
// table and tested as one.

TEST (RecMode, EveryModeWritesUnderTheFinger)
{
  EXPECT_TRUE (shouldWriteTick (RecMode::Touch, true, true));
  EXPECT_TRUE (shouldWriteTick (RecMode::Latch, true, true));
  EXPECT_TRUE (shouldWriteTick (RecMode::Write, true, true));
}

// Touch is punch-out: lift the finger and the pass leaves what earlier passes
// put there. This is what the engine did before there was a choice, and it
// stays the default.
TEST (RecMode, TouchWritesOnlyUnderTheFinger)
{
  EXPECT_FALSE (shouldWriteTick (RecMode::Touch, false, false));
  EXPECT_FALSE (shouldWriteTick (RecMode::Touch, false, true));
}

// Latch takes hold at the first touch and does not let go: after the finger
// lifts it carries on writing the position it was left at.
TEST (RecMode, LatchKeepsWritingOnceTouched)
{
  EXPECT_FALSE (shouldWriteTick (RecMode::Latch, false, false))
      << "nothing has been touched yet, so there is nothing to hold";
  EXPECT_TRUE (shouldWriteTick (RecMode::Latch, false, true));
}

// Write overwrites the whole pass whether it is touched or not — that is what
// makes it the one that clears an old take out of the way.
TEST (RecMode, WriteOverwritesEvenUntouched)
{
  EXPECT_TRUE (shouldWriteTick (RecMode::Write, false, false));
  EXPECT_TRUE (shouldWriteTick (RecMode::Write, false, true));
}

// Before the first touch, Latch and Write part company. Getting this the wrong
// way round would make Latch overwrite a take from its first tick, which is
// Write's job.
TEST (RecMode, LatchAndWriteDifferBeforeTheFirstTouch)
{
  EXPECT_NE (shouldWriteTick (RecMode::Latch, false, false),
             shouldWriteTick (RecMode::Write, false, false));
}

TEST (RecMode, NamesRoundTrip)
{
  for (auto const mode : { RecMode::Touch, RecMode::Latch,
                           RecMode::Write })
    EXPECT_EQ (recModeFromName (recModeName (mode)), mode);
}

// An unknown name is what a hand-edited or older settings file hands over.
// Falling back to the mode that behaves as this device always has is the only
// safe answer.
TEST (RecMode, AnUnknownNameFallsBackToTouch)
{
  EXPECT_EQ (recModeFromName ("Overdub"), RecMode::Touch);
  EXPECT_EQ (recModeFromName (""), RecMode::Touch);
}


// While a take runs you need to see what is still underneath it, or you are
// recording over something you cannot see. Not in Write, though: there the
// whole pass is replaced, so a ghost of the old one is noise beside the take
// rather than information about it.
TEST (RecMode, TouchAndLatchShowWhatIsBeingRecordedOver)
{
  EXPECT_TRUE (showsRecordingUnderlay (RecMode::Touch, true, true));
  EXPECT_TRUE (showsRecordingUnderlay (RecMode::Latch, true, true));
  EXPECT_FALSE (showsRecordingUnderlay (RecMode::Write, true, true));
}

TEST (RecMode, NothingIsShownWithoutATakeOrWithoutAnythingUnderneath)
{
  EXPECT_FALSE (showsRecordingUnderlay (RecMode::Touch, false, true))
      << "no take is running, so there is nothing being recorded over";
  EXPECT_FALSE (showsRecordingUnderlay (RecMode::Touch, true, false))
      << "the slot was empty, so there is nothing to show";
}

}

// ── Cycling through the modes ────────────────────────────────────────────

TEST (RecMode, CyclingVisitsEveryModeAndComesBack)
{
  // The panel key and the bar's key both step by one and wrap. If the index
  // lookup ever failed to find the current mode it would return 0 and every
  // press would land on the first mode -- a key that looks stuck rather than
  // one that errors.
  auto const count = static_cast<int> (recMenuModes.size ());
  ASSERT_GT (count, 1);

  std::set<RecMode> seen;
  auto mode = recMenuModes[0];

  for (int step = 0; step < count; ++step)
    {
      seen.insert (mode);
      mode = recMenuModes[static_cast<size_t> ((recMenuIndex (mode) + 1)
                                               % count)];
    }

  EXPECT_EQ (static_cast<int> (seen.size ()), count)
      << "every mode is reachable by pressing the key";
  EXPECT_EQ (mode, recMenuModes[0]) << "and it comes back round";
}

TEST (RecMode, EveryModeKnowsItsOwnPlaceInTheMenu)
{
  for (size_t i = 0; i < recMenuModes.size (); ++i)
    EXPECT_EQ (recMenuIndex (recMenuModes[i]), static_cast<int> (i));
}
