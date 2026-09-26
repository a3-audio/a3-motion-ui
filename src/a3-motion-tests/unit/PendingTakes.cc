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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-ui/PendingTakes.hh>

using namespace a3;

namespace
{
SlotContent
held (juce::String const &clip)
{
  return { std::make_shared<Pattern> (), juce::File ("/tmp/" + clip + ".json") };
}
}

// A finished take is not on disk yet, and the slot remembers what it held.
TEST (PendingTakes, ATakeMarksItsSlotAndRemembersWhatWasThere)
{
  PendingTakes takes (4, 2);
  auto const before = held ("Breath");

  takes.begin (1, 0, before);

  EXPECT_TRUE (takes.isPending (1, 0));
  EXPECT_FALSE (takes.isPending (1, 1));
  EXPECT_FALSE (takes.isPending (0, 0));
  EXPECT_EQ (takes.resolve (1, 0).pattern, before.pattern);
}

// A second take on a slot that is still unsaved goes back to the last *saved*
// state, not to the first take -- and a second take that ends empty leaves
// the first one standing, still unsaved.
TEST (PendingTakes, ASecondTakeKeepsTheOriginalBefore)
{
  PendingTakes takes (4, 2);
  auto const original = held ("Breath");
  auto const firstTake = held ("");

  takes.begin (0, 1, original);
  takes.begin (0, 1, firstTake);

  EXPECT_EQ (takes.resolve (0, 1).pattern, original.pattern);
}

TEST (PendingTakes, ResolvingClearsTheMark)
{
  PendingTakes takes (4, 2);
  takes.begin (2, 0, held ("Helix"));

  auto const back = takes.resolve (2, 0);

  EXPECT_FALSE (takes.isPending (2, 0));
  EXPECT_EQ (back.clipFile.getFileNameWithoutExtension (), "Helix");
}

// Saved: the take stays in the slot, only the mark goes.
TEST (PendingTakes, ClearingForgetsBefore)
{
  PendingTakes takes (4, 2);
  takes.begin (3, 1, held ("Wave"));

  takes.clear (3, 1);

  EXPECT_FALSE (takes.isPending (3, 1));
}

// A set names only what is on disk: an unsaved slot stands for what it held.
TEST (PendingTakes, ASetSeesWhatTheSlotHeldBefore)
{
  PendingTakes takes (4, 2);
  auto const before = held ("Breath");
  auto const take = held ("");
  takes.begin (0, 0, before);

  EXPECT_EQ (takes.forSet (0, 0, take).pattern, before.pattern);
  EXPECT_EQ (takes.forSet (0, 1, take).pattern, take.pattern);
}

// An empty slot before the take is an empty slot for the set too.
TEST (PendingTakes, AnEmptyBeforeIsAnEmptySlot)
{
  PendingTakes takes (4, 2);
  takes.begin (1, 1, SlotContent{});

  EXPECT_EQ (takes.forSet (1, 1, held ("")).pattern, nullptr);
}

TEST (PendingTakes, LoadingASetClearsEverySlot)
{
  PendingTakes takes (4, 2);
  takes.begin (0, 0, held ("A"));
  takes.begin (3, 1, held ("B"));

  takes.clearAll ();

  EXPECT_FALSE (takes.isPending (0, 0));
  EXPECT_FALSE (takes.isPending (3, 1));
}

// Discard costs the take, so it is asked twice -- like Delete in FILES.
TEST (PendingTakes, DiscardConfirmsOnlyOnTheSecondPress)
{
  PendingTakes takes (4, 2);
  takes.begin (0, 0, held ("A"));

  EXPECT_FALSE (takes.pressDiscard (0, 0));
  EXPECT_TRUE (takes.isDiscardArmed (0, 0));
  EXPECT_TRUE (takes.pressDiscard (0, 0));
  EXPECT_FALSE (takes.isDiscardArmed (0, 0));
}

// Armed on one slot, pressed on another: that is a first press there, not a
// confirmation of something the finger is no longer looking at.
TEST (PendingTakes, AnArmDoesNotCarryToAnotherSlot)
{
  PendingTakes takes (4, 2);
  takes.begin (0, 0, held ("A"));
  takes.begin (1, 0, held ("B"));

  EXPECT_FALSE (takes.pressDiscard (0, 0));
  EXPECT_FALSE (takes.pressDiscard (1, 0));
  EXPECT_FALSE (takes.isDiscardArmed (0, 0));
  EXPECT_TRUE (takes.isDiscardArmed (1, 0));
}

TEST (PendingTakes, TouchingAnythingElseDisarms)
{
  PendingTakes takes (4, 2);
  takes.begin (0, 0, held ("A"));
  takes.pressDiscard (0, 0);

  takes.disarm ();

  EXPECT_FALSE (takes.isDiscardArmed (0, 0));
  EXPECT_FALSE (takes.pressDiscard (0, 0));
}

// Nothing to discard on a saved slot: the key is ACT there, not DISCARD.
TEST (PendingTakes, ASlotWithoutATakeCannotBeArmed)
{
  PendingTakes takes (4, 2);

  EXPECT_FALSE (takes.pressDiscard (0, 0));
  EXPECT_FALSE (takes.isDiscardArmed (0, 0));
}

// Resolving -- by discard or by replacement -- also drops an arm on that slot.
TEST (PendingTakes, ResolvingDropsTheArm)
{
  PendingTakes takes (4, 2);
  takes.begin (0, 0, held ("A"));
  takes.pressDiscard (0, 0);

  takes.resolve (0, 0);

  EXPECT_FALSE (takes.isDiscardArmed (0, 0));
}

// Out of range is a slot without a take, not a crash.
TEST (PendingTakes, OutOfRangeIsNotPending)
{
  PendingTakes takes (4, 2);
  takes.begin (9, 9, held ("A"));

  EXPECT_FALSE (takes.isPending (9, 9));
  EXPECT_EQ (takes.resolve (9, 9).pattern, nullptr);
}

// SAVE and DISCARD are offered only while no take is running or waiting for
// its downbeat anywhere. The keys used to wear SAVE during a count-in while
// REC actually called the take off -- a key whose face and whose action
// disagree is the one you press wrong.
TEST (PendingTakes, TheKeysAreOfferedOnlyWhileNoTakeIsUnderway)
{
  PendingTakes takes (4, 2);
  takes.begin (0, 0, held ("A"));

  EXPECT_TRUE (takes.offersKeys (0, 0, false));
  EXPECT_FALSE (takes.offersKeys (0, 0, true));
  EXPECT_FALSE (takes.offersKeys (1, 0, false));
}

// A clip or shape renamed in FILES while a slot holds an unsaved take over it
// (#30): what the slot held before still names the old one, and a set written
// or a Discard pressed afterwards reached for a name and a file that were gone.
TEST (PendingTakes, ARenamedShapeIsRenamedInWhatASlotHeldBefore)
{
  PendingTakes takes (4, 2);
  auto before = held ("Wave");
  before.pattern->setName ("Wave");
  takes.begin (1, 1, before);

  takes.renamePattern ("Wave", "Swell");

  EXPECT_EQ (takes.forSet (1, 1, held ("take")).pattern->getName (), "Swell");
}

TEST (PendingTakes, AMovedClipFileIsFollowedByWhatASlotHeldBefore)
{
  PendingTakes takes (4, 2);
  takes.begin (0, 1, held ("Wave slow"));
  takes.begin (2, 0, held ("Other"));

  takes.moveClipFile (juce::File ("/tmp/Wave slow.json"),
                      juce::File ("/tmp/Swell slow.json"));

  EXPECT_EQ (takes.resolve (0, 1).clipFile,
             juce::File ("/tmp/Swell slow.json"));
  EXPECT_EQ (takes.resolve (2, 0).clipFile, juce::File ("/tmp/Other.json"))
      << "a slot that held something else is left alone";
}

TEST (PendingTakes, RenamingTouchesNothingThatIsNotPending)
{
  PendingTakes takes (4, 2);
  takes.renamePattern ("Wave", "Swell");
  takes.moveClipFile (juce::File ("/tmp/a.json"), juce::File ("/tmp/b.json"));
  EXPECT_FALSE (takes.isPending (0, 0));
}
