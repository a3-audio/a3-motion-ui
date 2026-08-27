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

struct SkinSelectionTest : public ::testing::Test
{
  juce::File dir;
  juce::File config;

  void
  SetUp () override
  {
    dir = juce::File::getSpecialLocation (
              juce::File::SpecialLocationType::tempDirectory)
              .getChildFile ("a3-skin-selection-test");
    dir.deleteRecursively ();
    dir.getChildFile ("skins").createDirectory ();

    config = dir.getChildFile ("config.json");
  }

  void
  TearDown () override
  {
    dir.deleteRecursively ();
  }

  void
  writeSkin (juce::String const &name)
  {
    dir.getChildFile ("skins")
        .getChildFile (name + ".json")
        .replaceWithText ("{}");
  }
};

TEST_F (SkinSelectionTest, TheSkinsFolderIsListedByName)
{
  writeSkin ("mono");
  writeSkin ("default");
  writeSkin ("neutral");

  auto const found = availableSkins (dir);

  ASSERT_EQ (found.size (), 3);
  EXPECT_EQ (found[0], "default") << "sorted, so the menu's order is stable "
                                     "across machines";
  EXPECT_EQ (found[1], "mono");
  EXPECT_EQ (found[2], "neutral");
}

// The menu has to show something even on a checkout where the folder is
// missing, and "default" is what skinFile() falls back to anyway.
TEST_F (SkinSelectionTest, AMissingFolderStillOffersTheDefault)
{
  dir.getChildFile ("skins").deleteRecursively ();

  auto const found = availableSkins (dir);

  ASSERT_EQ (found.size (), 1);
  EXPECT_EQ (found[0], "default");
}

TEST_F (SkinSelectionTest, OnlyJsonFilesCount)
{
  writeSkin ("default");
  dir.getChildFile ("skins").getChildFile ("notes.txt").replaceWithText ("x");
  dir.getChildFile ("skins").getChildFile ("default.json.bak")
      .replaceWithText ("{}");

  auto const found = availableSkins (dir);

  ASSERT_EQ (found.size (), 1);
  EXPECT_EQ (found[0], "default");
}

// config.json is hand-maintained: its ordering and its formatting are how a
// person reads it. Only the one value may move.
TEST_F (SkinSelectionTest, WritingTheSkinLeavesTheRestOfTheFileAlone)
{
  auto const before = juce::String (R"({
  "osc": {
    "host": "127.0.0.1"
  },

  "ui": {
    "sphereScale": 0.62,
    "skin": "default"
  }
})");
  config.replaceWithText (before, false, false, "\n");

  EXPECT_TRUE (writeActiveSkin (config, "mono"));

  auto const after = config.loadFileAsString ();
  EXPECT_EQ (after, before.replace ("\"skin\": \"default\"",
                                    "\"skin\": \"mono\""));
}

TEST_F (SkinSelectionTest, TheWrittenSkinIsTheOneThatLoads)
{
  config.replaceWithText (R"({ "ui": { "skin": "default" } })");
  dir.getChildFile ("skins")
      .getChildFile ("mono.json")
      .replaceWithText (R"({ "accent": { "r": 1, "g": 2, "b": 3 } })");

  ASSERT_TRUE (writeActiveSkin (config, "mono"));

  auto const config2
      = juce::JSON::parse (config.loadFileAsString ());
  auto const theme = loadTheme (loadActiveSkinVar (config, config2));

  EXPECT_EQ (theme.accent.r, 1);
  EXPECT_EQ (theme.accent.g, 2);
  EXPECT_EQ (theme.accent.b, 3);
}

// Refusing beats inventing structure: a config without the key is one this
// code does not understand, and guessing where "ui" should go would write a
// file the next hand edit fights with.
TEST_F (SkinSelectionTest, AConfigWithoutTheKeyIsNotRewritten)
{
  auto const before = juce::String (R"({ "osc": { "host": "127.0.0.1" } })");
  config.replaceWithText (before, false, false, "\n");

  EXPECT_FALSE (writeActiveSkin (config, "mono"));
  EXPECT_EQ (config.loadFileAsString (), before);
}

TEST_F (SkinSelectionTest, WritingTheSkinItAlreadyHasIsHarmless)
{
  auto const before = juce::String (R"({ "ui": { "skin": "mono" } })");
  config.replaceWithText (before, false, false, "\n");

  EXPECT_TRUE (writeActiveSkin (config, "mono"));
  EXPECT_EQ (config.loadFileAsString (), before);
}

TEST_F (SkinSelectionTest, TheActiveSkinIsReadBackByName)
{
  config.replaceWithText (R"({ "ui": { "skin": "neutral" } })");

  EXPECT_EQ (activeSkinName (config), "neutral");
}

// An empty or absent entry means the default skin, the same way skinFile()
// reads it — the menu must land on the entry that is actually in force.
TEST_F (SkinSelectionTest, AnAbsentEntryReadsAsTheDefault)
{
  config.replaceWithText (R"({ "ui": { } })");

  EXPECT_EQ (activeSkinName (config), "default");
}

}
