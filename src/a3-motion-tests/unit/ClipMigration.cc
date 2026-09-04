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

#include <JuceHeader.h>

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/ClipMigration.hh>

using namespace a3;

namespace
{
juce::File
aRootWithAnOldTake (juce::String const &dirName)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile (dirName);
  root.deleteRecursively ();
  root.getChildFile ("user").createDirectory ();

  root.getChildFile ("user/04_Rec_120613.svg")
      .replaceWithText (
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-1 -1 2 2\""
          " data-name=\"Rec_120613\" data-beats=\"4\" data-ppqn=\"128\""
          " data-spin=\"3\" data-end-action=\"bounce\" data-fade=\"8\""
          " data-reach=\"0.4\" data-act-mode=\"hold\">"
          "<path d=\"M -0.5 0 C -0.25 0.5 0.25 -0.5 0.5 0\"/></svg>");

  return root;
}
}

// An old file carries its settings inside the SVG. The migration lifts them
// out into a clip file beside it.
TEST (ClipMigration, AnOldTakeGetsAClipOfItsOwn)
{
  auto const root = aRootWithAnOldTake ("a3-migration");

  EXPECT_EQ (migrateCombinedPatterns (root), 1);

  auto const clip
      = ClipFile::load (root.getChildFile ("clips/Rec_120613.json"));
  ASSERT_TRUE (clip.has_value ());
  EXPECT_EQ (clip->name, "Rec_120613");
  // The clip is named without the beat-count prefix and points at the file
  // with it -- the same rule the factory clips and saveUserPattern() follow,
  // and the one PatternLibrary looks the clip up by. Getting this wrong makes
  // every migrated take invisible, which is exactly what happened.
  EXPECT_EQ (clip->svg, "04_Rec_120613");
  EXPECT_EQ (clip->settings.spin, 3);
  EXPECT_EQ (clip->settings.endAction, EndAction::Bounce);
  EXPECT_EQ (clip->settings.actMode, ActMode::Hold);
  EXPECT_FLOAT_EQ (clip->settings.reach, 0.4f);

  root.deleteRecursively ();
}

// The fade is baked into the old geometry and cannot be recovered, so the clip
// claims none: the take keeps sounding exactly as it did, and nothing pretends
// the fade is still adjustable when the material it ate is gone.
TEST (ClipMigration, AMigratedTakeClaimsNoFade)
{
  auto const root = aRootWithAnOldTake ("a3-migration-fade");

  ASSERT_EQ (migrateCombinedPatterns (root), 1);

  auto const clip
      = ClipFile::load (root.getChildFile ("clips/Rec_120613.json"));
  ASSERT_TRUE (clip.has_value ());
  EXPECT_EQ (clip->settings.fadeSixteenths, 0);

  root.deleteRecursively ();
}

// A migration that deletes nothing can be run again when it goes wrong. The
// shape also stays in user/: moving the folders is a later step, and
// PatternLibrary looks in user/, so a shape moved now would vanish.
TEST (ClipMigration, TheOldFileIsLeftExactlyWhereItWas)
{
  auto const root = aRootWithAnOldTake ("a3-migration-keep");
  auto const old = root.getChildFile ("user/04_Rec_120613.svg");
  auto const before = old.loadFileAsString ();

  ASSERT_EQ (migrateCombinedPatterns (root), 1);

  ASSERT_TRUE (old.existsAsFile ()) << "the migration deleted the old take";
  EXPECT_EQ (old.loadFileAsString (), before)
      << "the migration rewrote the old take";

  root.deleteRecursively ();
}

// Running it twice must not write a second time -- it runs on every start.
TEST (ClipMigration, RunningItAgainDoesNothing)
{
  auto const root = aRootWithAnOldTake ("a3-migration-twice");

  ASSERT_EQ (migrateCombinedPatterns (root), 1);
  EXPECT_EQ (migrateCombinedPatterns (root), 0);

  root.deleteRecursively ();
}

// A clip already written by hand is not overwritten by a migration that
// happens to run afterwards: what somebody set beats what a file once held.
TEST (ClipMigration, AnExistingClipIsLeftAlone)
{
  auto const root = aRootWithAnOldTake ("a3-migration-existing");

  Clip mine;
  mine.name = "Rec_120613";
  mine.svg = "04_Rec_120613";
  mine.settings.spin = -5;
  ASSERT_TRUE (
      ClipFile::save (mine, root.getChildFile ("clips/Rec_120613.json")));

  EXPECT_EQ (migrateCombinedPatterns (root), 0);

  auto const clip
      = ClipFile::load (root.getChildFile ("clips/Rec_120613.json"));
  ASSERT_TRUE (clip.has_value ());
  EXPECT_EQ (clip->settings.spin, -5);

  root.deleteRecursively ();
}

TEST (ClipMigration, AFolderWithNoTakesIsNotAnError)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-migration-empty");
  root.deleteRecursively ();
  root.createDirectory ();

  EXPECT_EQ (migrateCombinedPatterns (root), 0);

  root.deleteRecursively ();
}

// Deleting a clip is a decision, and a restart must not undo it. Guarding on
// "is the clip file there?" alone cannot tell a clip that was never made from
// one that was thrown away -- so the migration writes down what it has already
// handled and never touches those takes again.
TEST (ClipMigration, ADeletedClipStaysDeleted)
{
  auto const root = aRootWithAnOldTake ("a3-migration-deleted");

  ASSERT_EQ (migrateCombinedPatterns (root), 1);

  auto const clip = root.getChildFile ("clips/Rec_120613.json");
  ASSERT_TRUE (clip.deleteFile ());

  EXPECT_EQ (migrateCombinedPatterns (root), 0);
  EXPECT_FALSE (clip.existsAsFile ())
      << "the migration put back a clip the user deleted";
}

// A take that arrives later -- a stick plugged in, a backup copied over -- is
// not covered by what was written down, so it still gets its clip.
TEST (ClipMigration, ATakeThatArrivesLaterIsStillMigrated)
{
  auto const root = aRootWithAnOldTake ("a3-migration-later");

  ASSERT_EQ (migrateCombinedPatterns (root), 1);

  root.getChildFile ("user/08_Rec_991122.svg")
      .replaceWithText (
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-1 -1 2 2\""
          " data-name=\"Rec_991122\" data-beats=\"8\" data-ppqn=\"128\">"
          "<path d=\"M -0.5 0 L 0.5 0\"/></svg>");

  EXPECT_EQ (migrateCombinedPatterns (root), 1);
  EXPECT_TRUE (
      root.getChildFile ("clips/Rec_991122.json").existsAsFile ());
}
