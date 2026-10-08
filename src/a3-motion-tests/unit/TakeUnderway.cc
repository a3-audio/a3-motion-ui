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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-ui/TakeUnderway.hh>

using namespace a3;

// A take from REC until it ends: which slot, the take, and what the slot held.
// One place decides what each way out puts back (2026-10-08) -- a take whose
// slot was filled with something else used to leave the lock and REC's "end
// take" standing, and the next REC put the old clip back over the new one.

namespace
{
SlotContent
held (juce::String const &clip)
{
  return { std::make_shared<Pattern> (),
           juce::File ("/tmp/" + clip + ".json") };
}
}

TEST (TakeUnderway, ATakeIsOnItsSlotUntilItEnds)
{
  TakeUnderway underway;
  EXPECT_FALSE (underway.any ());

  auto const take = std::make_shared<Pattern> ();
  underway.begin (2, 0, take, held ("Breath"));
  EXPECT_TRUE (underway.any ());
  EXPECT_TRUE (underway.isOn (2, 0));
  EXPECT_TRUE (underway.isOnChannel (2));
  EXPECT_FALSE (underway.isOn (1, 0));
  EXPECT_EQ (underway.take (), take);
}

/** Calling off before the downbeat puts back what the slot held -- if the
 *  slot still holds the take. */
TEST (TakeUnderway, CalledOffPutsBackWhatTheSlotHeld)
{
  TakeUnderway underway;
  auto const take = std::make_shared<Pattern> ();
  auto const before = held ("Breath");
  underway.begin (0, 0, take, before);

  auto const back = underway.calledOff (take);
  ASSERT_TRUE (back.has_value ());
  EXPECT_EQ (back->pattern, before.pattern);
  EXPECT_FALSE (underway.any ());
}

/** ...and nothing over a clip that has been put there since. */
TEST (TakeUnderway, CalledOffPutsNothingOverANewClip)
{
  TakeUnderway underway;
  auto const take = std::make_shared<Pattern> ();
  underway.begin (0, 0, take, held ("Breath"));

  EXPECT_FALSE (underway.calledOff (std::make_shared<Pattern> ()).has_value ());
  EXPECT_FALSE (underway.any ());
}

/** A shape, a clip with its figure or a set filling the slot ends the take
 *  there: the caller gets the take back to stop it, nothing is restored, and
 *  REC is REC again. Another slot leaves it alone. */
TEST (TakeUnderway, ReplacingTheSlotEndsTheTake)
{
  TakeUnderway underway;
  auto const take = std::make_shared<Pattern> ();
  underway.begin (0, 0, take, held ("Breath"));

  EXPECT_EQ (underway.slotReplaced (1, 0), nullptr);
  EXPECT_TRUE (underway.any ());

  EXPECT_EQ (underway.slotReplaced (0, 0), take);
  EXPECT_FALSE (underway.any ());
  EXPECT_FALSE (underway.isOnChannel (0));
}

/** A take that ran ends with what the slot held before it, for PendingTakes. */
TEST (TakeUnderway, EndingHandsBackWhatTheSlotHeld)
{
  TakeUnderway underway;
  auto const before = held ("Breath");
  underway.begin (0, 0, std::make_shared<Pattern> (), before);

  EXPECT_EQ (underway.before ().pattern, before.pattern);
  auto const ended = underway.ended ();
  EXPECT_EQ (ended.pattern, before.pattern);
  EXPECT_EQ (ended.clipFile, before.clipFile);
  EXPECT_FALSE (underway.any ());
}

/** One take at a time (2026-10-08): while one is scheduled or running, REC
 *  anywhere is refused, and so is REC on another slot while one is unsaved --
 *  a second take used to overwrite the first's record, which could come back
 *  into its slot band-locked with the old clip lost. The answer is the channel
 *  the take is on, for the readout. A second take on the slot of an unsaved
 *  one is still allowed: it keeps the first's before (PendingTakes::begin). */
TEST (TakeUnderway, OneTakeAtATime)
{
  TakeUnderway underway;
  PendingTakes pending (4, 2);

  EXPECT_FALSE (underway.refusesANewTake (1, 0, pending).has_value ());

  underway.begin (2, 0, std::make_shared<Pattern> (), held ("Breath"));
  EXPECT_EQ (underway.refusesANewTake (1, 0, pending), index_t{ 2 });
  EXPECT_EQ (underway.refusesANewTake (2, 0, pending), index_t{ 2 });

  underway.ended ();
  pending.begin (2, 0, held ("Breath"));
  EXPECT_EQ (underway.refusesANewTake (1, 0, pending), index_t{ 2 })
      << "another slot while a take is unsaved";
  EXPECT_FALSE (underway.refusesANewTake (2, 0, pending).has_value ())
      << "again on the unsaved take's own slot";

  pending.clear (2, 0);
  EXPECT_FALSE (underway.refusesANewTake (1, 0, pending).has_value ());
}

/** A set load replaces every slot, so it ends a take wherever it is -- on a
 *  slot the new set leaves empty too -- and hands it out to be stopped. */
TEST (TakeUnderway, ASetLoadEndsATakeWhereverItIs)
{
  TakeUnderway underway;
  EXPECT_EQ (underway.everythingReplaced (), nullptr);

  auto const take = std::make_shared<Pattern> ();
  underway.begin (3, 1, take, held ("Breath"));
  EXPECT_EQ (underway.everythingReplaced (), take);
  EXPECT_FALSE (underway.any ());
}
