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

#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

namespace
{

struct SkinFilesTest : public ::testing::Test
{
  juce::File dir;

  void
  SetUp () override
  {
    dir = juce::File::getSpecialLocation (
              juce::File::SpecialLocationType::tempDirectory)
              .getChildFile ("a3-skin-files-test");
    dir.deleteRecursively ();
    dir.getChildFile ("skins").createDirectory ();
    dir.getChildFile ("config.json")
        .replaceWithText (R"({ "ui": { "skin": "default" } })");
  }

  void
  TearDown () override
  {
    dir.deleteRecursively ();
  }

  void
  writeSkin (juce::String const &name, juce::String const &body = "{}")
  {
    dir.getChildFile ("skins").getChildFile (name + ".json")
        .replaceWithText (body);
  }

  bool
  exists (juce::String const &name)
  {
    return dir.getChildFile ("skins")
        .getChildFile (name + ".json")
        .existsAsFile ();
  }
};

// A name is a file name and a key in config.json, so what a person can dial in
// on an encoder has to be narrower than what they might dial in by accident.

TEST_F (SkinFilesTest, ANameIsLowercaseLettersDigitsAndDashes)
{
  EXPECT_TRUE (isUsableSkinName ("neutral"));
  EXPECT_TRUE (isUsableSkinName ("neutral-2"));
  EXPECT_TRUE (isUsableSkinName ("stage3"));

  EXPECT_FALSE (isUsableSkinName ("")) << "a nameless file helps nobody";
  EXPECT_FALSE (isUsableSkinName ("../escape")) << "not a path";
  EXPECT_FALSE (isUsableSkinName ("Neutral")) << "one spelling per name";
  EXPECT_FALSE (isUsableSkinName ("my skin")) << "no spaces in a file name";
}

TEST_F (SkinFilesTest, ACopyIsOfferedTheNextFreeNumber)
{
  writeSkin ("neutral");

  EXPECT_EQ (nextFreeSkinName (dir, "neutral"), "neutral-2");

  writeSkin ("neutral-2");

  EXPECT_EQ (nextFreeSkinName (dir, "neutral"), "neutral-3");
}

// Copying "neutral-2" must not produce "neutral-2-2": the number is a counter,
// not part of the name.
TEST_F (SkinFilesTest, ANumberedNameCountsOnRatherThanNesting)
{
  writeSkin ("neutral");
  writeSkin ("neutral-2");

  EXPECT_EQ (nextFreeSkinName (dir, "neutral-2"), "neutral-3");
}

TEST_F (SkinFilesTest, RenamingMovesTheFile)
{
  writeSkin ("neutral", R"({ "accent": { "r": 7 } })");

  EXPECT_TRUE (renameSkin (dir, "neutral", "stage"));

  EXPECT_FALSE (exists ("neutral"));
  ASSERT_TRUE (exists ("stage"));
  EXPECT_TRUE (dir.getChildFile ("skins")
                   .getChildFile ("stage.json")
                   .loadFileAsString ()
                   .contains ("\"r\": 7"));
}

// Renaming the skin that is running has to take config.json with it, or the
// next start falls back to the built-in defaults and the work looks lost.
TEST_F (SkinFilesTest, RenamingTheActiveSkinFollowsInTheConfig)
{
  writeSkin ("default");
  dir.getChildFile ("config.json")
      .replaceWithText (R"({ "ui": { "skin": "default" } })");

  ASSERT_TRUE (renameSkin (dir, "default", "house"));

  EXPECT_EQ (activeSkinName (dir.getChildFile ("config.json")), "house");
}

TEST_F (SkinFilesTest, RenamingOntoAnExistingNameIsRefused)
{
  writeSkin ("neutral");
  writeSkin ("mono");

  EXPECT_FALSE (renameSkin (dir, "neutral", "mono"));
  EXPECT_TRUE (exists ("neutral")) << "and nothing is lost in the attempt";
  EXPECT_TRUE (exists ("mono"));
}

TEST_F (SkinFilesTest, RenamingToAnUnusableNameIsRefused)
{
  writeSkin ("neutral");

  EXPECT_FALSE (renameSkin (dir, "neutral", "../oops"));
  EXPECT_TRUE (exists ("neutral"));
}

TEST_F (SkinFilesTest, DeletingRemovesTheFile)
{
  writeSkin ("default");
  writeSkin ("scratch");

  EXPECT_TRUE (deleteSkin (dir, "scratch"));
  EXPECT_FALSE (exists ("scratch"));
}

// Deleting what is running would leave the config pointing at nothing, so it
// falls back to another skin — and to the built-in defaults only if there is
// no other.
TEST_F (SkinFilesTest, DeletingTheActiveSkinLeavesAnotherOneRunning)
{
  writeSkin ("default");
  writeSkin ("scratch");
  dir.getChildFile ("config.json")
      .replaceWithText (R"({ "ui": { "skin": "scratch" } })");

  ASSERT_TRUE (deleteSkin (dir, "scratch"));

  EXPECT_EQ (activeSkinName (dir.getChildFile ("config.json")), "default");
}

// The last one stays. An empty folder is a device with no look at all, and
// getting back out of that needs a file manager.
TEST_F (SkinFilesTest, TheLastSkinCannotBeDeleted)
{
  writeSkin ("default");

  EXPECT_FALSE (deleteSkin (dir, "default"));
  EXPECT_TRUE (exists ("default"));
}

}
