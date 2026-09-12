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

#include <a3-motion-engine/SplitFolder.hh>

using namespace a3;

namespace
{
/** A folder split the way the shapes' one already is, filled by hand. */
struct Fixture
{
  juce::File root;

  Fixture ()
      : root (juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("a3-split-"
                                 + juce::String (juce::Random::getSystemRandom ()
                                                     .nextInt (1000000))))
  {
    root.getChildFile ("system").createDirectory ();
    root.getChildFile ("user").createDirectory ();
  }

  ~Fixture () { root.deleteRecursively (); }

  void put (juce::String const &where, juce::String const &name)
  {
    root.getChildFile (where).getChildFile (name + ".scd").replaceWithText ("x");
  }
};
}

TEST (SplitFolder, ANameIsFoundAmongTheShippedOnes)
{
  Fixture f;
  f.put ("system", "Bloom");

  EXPECT_EQ (namedFileIn (f.root, "Bloom", ".scd"),
             f.root.getChildFile ("system").getChildFile ("Bloom.scd"));
}

TEST (SplitFolder, ANameIsFoundAmongYourOwn)
{
  Fixture f;
  f.put ("user", "Mine");

  EXPECT_EQ (namedFileIn (f.root, "Mine", ".scd"),
             f.root.getChildFile ("user").getChildFile ("Mine.scd"));
}

// Yours wins, so one of the instrument's own can be replaced by a version of
// your own without deleting the original -- which stays there to go back to.
TEST (SplitFolder, YourOwnCoversAShippedOneOfTheSameName)
{
  Fixture f;
  f.put ("system", "Bloom");
  f.put ("user", "Bloom");

  EXPECT_EQ (namedFileIn (f.root, "Bloom", ".scd"),
             f.root.getChildFile ("user").getChildFile ("Bloom.scd"));
}

// A name with nothing behind it answers with a file that does not exist,
// rather than with something that happens to be there.
TEST (SplitFolder, ANameWithNothingBehindItFindsNothing)
{
  Fixture f;
  EXPECT_FALSE (namedFileIn (f.root, "Nobody", ".scd").existsAsFile ());
}

// Everything new is the performer's. Nothing the device writes may land among
// the instrument's own.
TEST (SplitFolder, SomethingNewGoesAmongYourOwn)
{
  Fixture f;
  EXPECT_EQ (newFileIn (f.root, "Fresh", ".scd"),
             f.root.getChildFile ("user").getChildFile ("Fresh.scd"));
}

// Listing carries where each came from, because that is the whole point: the
// filter reads it, and so does the rule that keeps a shipped one from being
// written over.
TEST (SplitFolder, ListingSaysWhereEachOneCameFrom)
{
  Fixture f;
  f.put ("system", "Bloom");
  f.put ("user", "Mine");

  auto const listed = listFilesIn (f.root, ".scd");
  ASSERT_EQ (listed.size (), 2u);

  auto const bloom = std::find_if (listed.begin (), listed.end (),
                                   [] (SplitFolderEntry const &e) {
                                     return e.name == "Bloom";
                                   });
  auto const mine = std::find_if (listed.begin (), listed.end (),
                                  [] (SplitFolderEntry const &e) {
                                    return e.name == "Mine";
                                  });

  ASSERT_NE (bloom, listed.end ());
  ASSERT_NE (mine, listed.end ());
  EXPECT_TRUE (bloom->isSystem);
  EXPECT_FALSE (mine->isSystem);
}

// One name, one row: the covered one is not listed twice. Which of the two
// is listed is the one that would be opened.
TEST (SplitFolder, ACoveredNameIsListedOnce)
{
  Fixture f;
  f.put ("system", "Bloom");
  f.put ("user", "Bloom");

  auto const listed = listFilesIn (f.root, ".scd");
  ASSERT_EQ (listed.size (), 1u);
  EXPECT_FALSE (listed[0].isSystem);
}

// The list is sorted by the name that is shown, the way the library's is.
TEST (SplitFolder, TheListIsSortedByName)
{
  Fixture f;
  f.put ("user", "Zebra");
  f.put ("system", "Alpha");
  f.put ("user", "Middle");

  auto const listed = listFilesIn (f.root, ".scd");
  ASSERT_EQ (listed.size (), 3u);
  EXPECT_EQ (listed[0].name, "Alpha");
  EXPECT_EQ (listed[1].name, "Middle");
  EXPECT_EQ (listed[2].name, "Zebra");
}

// A new name has to be free in both halves. Counting only against your own
// would hand back a name a shipped file already has -- and then namedFileIn
// would open yours where the list showed theirs, or the other way round
// depending on which half the reader looked in first.
TEST (SplitFolder, AFreeNameStepsPastBothHalves)
{
  Fixture f;
  f.put ("system", "Action");
  f.put ("user", "Action 2");

  EXPECT_EQ (freeNameIn (f.root, "Action", ".scd"), "Action 3");
}

TEST (SplitFolder, AnUntakenNameIsHandedBackAsItIs)
{
  Fixture f;
  EXPECT_EQ (freeNameIn (f.root, "Action", ".scd"), "Action");
}

// ── Moving an unsplit folder into its two halves ────────────────────────

// What is already there is the performer's. The instrument's own arrive in
// system/ because the repository puts them there -- git knows what shipped,
// and the device cannot. So anything found lying flat is theirs.
TEST (SplitFolder, LooseFilesBecomeYourOwn)
{
  Fixture f;
  f.root.getChildFile ("Set 2.json").replaceWithText ("x");

  EXPECT_EQ (splitLooseFilesIn (f.root, ".json"), 1);
  EXPECT_TRUE (
      f.root.getChildFile ("user").getChildFile ("Set 2.json").existsAsFile ());
  EXPECT_FALSE (f.root.getChildFile ("Set 2.json").existsAsFile ());
}

// Run again and it does nothing: there is nothing lying flat any more. A
// migration that is not idempotent is one that cannot be run on every start,
// and one that is not run on every start is one somebody has to remember.
TEST (SplitFolder, RunningItAgainMovesNothing)
{
  Fixture f;
  f.root.getChildFile ("Set 2.json").replaceWithText ("x");

  ASSERT_EQ (splitLooseFilesIn (f.root, ".json"), 1);
  EXPECT_EQ (splitLooseFilesIn (f.root, ".json"), 0);
}

// A name that is already there among the instrument's own is not overwritten
// and not lost: it lands beside it, counted, and both are reachable.
TEST (SplitFolder, ALooseNameThatClashesIsCountedRatherThanLost)
{
  Fixture f;
  f.put ("system", "Bloom");
  f.root.getChildFile ("Bloom.scd").replaceWithText ("mine");

  ASSERT_EQ (splitLooseFilesIn (f.root, ".scd"), 1);
  EXPECT_TRUE (f.root.getChildFile ("system")
                   .getChildFile ("Bloom.scd")
                   .existsAsFile ());
  EXPECT_TRUE (
      f.root.getChildFile ("user").getChildFile ("Bloom 2.scd").existsAsFile ());
}

// The two halves are made if they are not there. A device that has never seen
// this has neither.
TEST (SplitFolder, TheHalvesAreMadeIfMissing)
{
  auto const root
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-split-fresh-"
                           + juce::String (
                               juce::Random::getSystemRandom ().nextInt (1000000)));
  root.createDirectory ();
  root.getChildFile ("Loose.scd").replaceWithText ("x");

  EXPECT_EQ (splitLooseFilesIn (root, ".scd"), 1);
  EXPECT_TRUE (root.getChildFile ("user").isDirectory ());

  root.deleteRecursively ();
}

// Only the extension asked for. A folder may hold other things -- a readme, a
// ledger -- and a migration that swept those up would be one nobody could
// leave a note in.
TEST (SplitFolder, OnlyTheExtensionAskedForIsMoved)
{
  Fixture f;
  f.root.getChildFile ("Set.json").replaceWithText ("x");
  f.root.getChildFile ("README.md").replaceWithText ("x");

  EXPECT_EQ (splitLooseFilesIn (f.root, ".json"), 1);
  EXPECT_TRUE (f.root.getChildFile ("README.md").existsAsFile ());
}
