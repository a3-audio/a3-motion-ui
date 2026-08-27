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

#include <a3-motion-ui/ConfigFileWatcher.hh>

using namespace a3;

namespace
{

juce::File
tempFile (juce::String const &name)
{
  return juce::File::getSpecialLocation (
             juce::File::SpecialLocationType::tempDirectory)
      .getChildFile (name);
}

TEST (ConfigFileWatcher, UnchangedFileReportsNoChange)
{
  auto const file = tempFile ("a3-watcher-unchanged.json");
  file.replaceWithText ("{}");

  ConfigFileWatcher watcher{ file };

  EXPECT_FALSE (watcher.hasChanged ());
  EXPECT_FALSE (watcher.hasChanged ());

  file.deleteFile ();
}

TEST (ConfigFileWatcher, RewrittenFileReportsChangeExactlyOnce)
{
  auto const file = tempFile ("a3-watcher-rewritten.json");
  file.replaceWithText ("{}");

  ConfigFileWatcher watcher{ file };
  ASSERT_FALSE (watcher.hasChanged ());

  file.replaceWithText (R"({"corona": {"sizeMin": 1.4}})");

  EXPECT_TRUE (watcher.hasChanged ()) << "edit was not picked up";
  EXPECT_FALSE (watcher.hasChanged ()) << "same edit reported twice";

  file.deleteFile ();
}

// Tuning means saving the file repeatedly, sometimes while an editor writes a
// truncated intermediate state. A missing file must not throw or spin.
TEST (ConfigFileWatcher, MissingFileReportsNoChange)
{
  auto const file = tempFile ("a3-watcher-missing.json");
  file.deleteFile ();

  ConfigFileWatcher watcher{ file };

  EXPECT_FALSE (watcher.hasChanged ());
}


// Two files are watched now: config.json for operational changes including
// which skin is active, and the active skin itself for tuning. They have to
// report independently, or a tuning save would be missed while config.json
// sits still.
TEST (ConfigFileWatcher, TwoWatchersReportIndependently)
{
  auto const configLike = tempFile ("a3-watcher-two-config.json");
  auto const skinLike = tempFile ("a3-watcher-two-skin.json");
  configLike.replaceWithText ("{}");
  skinLike.replaceWithText ("{}");

  ConfigFileWatcher configWatcher{ configLike };
  ConfigFileWatcher skinWatcher{ skinLike };

  skinLike.replaceWithText (R"({"accent": {"r": 1, "g": 2, "b": 3}})");

  EXPECT_TRUE (skinWatcher.hasChanged ()) << "the skin edit was missed";
  EXPECT_FALSE (configWatcher.hasChanged ())
      << "the config reported a change it did not have";

  configLike.deleteFile ();
  skinLike.deleteFile ();
}

// Switching ui.skin re-points the second watcher. The old file must go quiet
// and the new one must be heard.
TEST (ConfigFileWatcher, RepointingFollowsTheNewFile)
{
  auto const first = tempFile ("a3-watcher-skin-first.json");
  auto const second = tempFile ("a3-watcher-skin-second.json");
  first.replaceWithText ("{}");
  second.replaceWithText ("{}");

  ConfigFileWatcher watcher{ first };
  watcher = ConfigFileWatcher{ second };

  first.replaceWithText (R"({"accent": {"r": 9, "g": 9, "b": 9}})");
  EXPECT_FALSE (watcher.hasChanged ()) << "still listening to the old skin";

  second.replaceWithText (R"({"accent": {"r": 8, "g": 8, "b": 8}})");
  EXPECT_TRUE (watcher.hasChanged ()) << "not listening to the new skin";

  first.deleteFile ();
  second.deleteFile ();
}

}
