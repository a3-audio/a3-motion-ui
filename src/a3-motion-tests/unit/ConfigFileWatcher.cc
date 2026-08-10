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

}
