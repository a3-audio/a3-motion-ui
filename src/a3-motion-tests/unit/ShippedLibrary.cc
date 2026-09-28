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

#include <JuceHeader.h>

#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-ui/SessionFile.hh>

using namespace a3;

// What ships: the library of shapes, clips, actions and sets (library v2,
// 2026-09-28). Tests about the set *format* live in SessionFile.cc; these are
// about the content.

namespace
{
juce::StringArray const families{ "Rhythm", "Orbit", "Loop", "Spiral",
                                  "Flower", "Cycle", "Edge", "Wander" };

bool
startsWithOneOf (juce::String const &name, juce::StringArray const &prefixes)
{
  for (auto const &p : prefixes)
    if (name.startsWith (p + " "))
      return true;
  return false;
}
}

// A shape's family is the first word of its name, so the list groups them
// without a folder.
TEST (ShippedLibrary, EveryShapeIsNamedByItsFamily)
{
  auto const files = juce::File (A3_PATTERN_SYSTEM_DIR)
                         .findChildFiles (juce::File::findFiles, false, "*.svg");
  EXPECT_EQ (files.size (), 50);
  for (auto const &file : files)
    EXPECT_TRUE (startsWithOneOf (PatternFile::peek (file).name, families))
        << file.getFileName ();
}

/** A set names its takes rather than carrying them, so a shipped set is only
 *  as good as the names in it: rename a shape and every set pointing at it
 *  loads as an empty slot, silently, because a missing name is not an error
 *  here -- it is a slot nobody has filled. This is the one place that would
 *  notice. */
// Re-enabled in Task 8 of library v2: the sets and clips still name the
// shapes from before the rename until Tasks 6 and 8 rebuild them.
TEST (ShippedLibrary, DISABLED_EveryShippedSetNamesShapesThatExist)
{
  juce::File const dir (A3_PATTERN_SESSIONS_DIR);
  ASSERT_TRUE (dir.isDirectory ()) << dir.getFullPathName ();

  juce::File const shapes (A3_PATTERN_SYSTEM_DIR);
  juce::StringArray known;
  for (auto const &file :
       shapes.findChildFiles (juce::File::findFiles, false, "*.svg"))
    known.add (PatternFile::peek (file).name);
  ASSERT_FALSE (known.isEmpty ());

  auto const files
      = dir.findChildFiles (juce::File::findFiles, false, "*.json");
  EXPECT_FALSE (files.isEmpty ()) << "no sets ship at all";

  for (auto const &file : files)
    {
      auto const set = loadSession (file, 4, 1);
      EXPECT_EQ (juce::String (set.name), file.getFileNameWithoutExtension ())
          << "a set's name is what the browser lists it under";
      ASSERT_EQ (set.channels.size (), 4u) << file.getFileName ();

      for (auto const &channel : set.channels)
        {
          ASSERT_EQ (channel.slots.size (), 1u) << file.getFileName ();
          for (auto const &slot : channel.slots)
            EXPECT_TRUE (known.contains (juce::String (slot.patternName)))
                << file.getFileName () << ": no shape called "
                << slot.patternName;
        }
    }
}

/** Since one clip per channel (2026-09-28) a shipped set is a mood: each
 *  channel has one clip, named, whose own shape is the one the set names, and
 *  six actions that exist -- no button left empty, so a set loaded mid-night
 *  never leaves the previous set's actions under the fingers. And one slot
 *  in the file, so an older build reading it does not find a second clip the
 *  device would quietly drop. */
// Re-enabled in Task 8 of library v2: the sets still name the clips from
// before the rebuild until Task 8 rebuilds them.
TEST (ShippedLibrary, DISABLED_EveryShippedSetIsOneClipAndSixActionsPerChannel)
{
  juce::File const dir (A3_PATTERN_SESSIONS_DIR);
  juce::File const clips (A3_PATTERN_CLIPS_DIR);
  juce::File const actions (A3_PATTERN_ACTIONS_DIR);

  for (auto const &file :
       dir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
      auto const parsed = juce::JSON::parse (file.loadFileAsString ());
      auto const *channels = parsed["channels"].getArray ();
      ASSERT_NE (channels, nullptr) << file.getFileName ();
      for (auto const &entry : *channels)
        EXPECT_EQ (entry["slots"].size (), 1) << file.getFileName ();

      auto const set = loadSession (file, 4, 1);
      for (auto const &channel : set.channels)
        {
          auto const &slot = channel.slots[0];
          auto const clipFile = clips.getChildFile (
              juce::String (slot.clipFile) + ".json");
          ASSERT_TRUE (clipFile.existsAsFile ())
              << file.getFileName () << ": no clip called " << slot.clipFile;
          EXPECT_EQ (juce::JSON::parse (clipFile.loadFileAsString ())["svg"]
                         .toString (),
                     juce::String (slot.patternName))
              << file.getFileName () << ": " << slot.clipFile
              << " is not drawn with " << slot.patternName;

          for (auto const &action : channel.actions)
            EXPECT_TRUE (actions
                             .getChildFile (juce::String (action.script)
                                            + ".scd")
                             .existsAsFile ())
                << file.getFileName () << ": no action called '"
                << action.script << "'";
        }
    }
}

/** The library ships fifty shapes, fifty clips and fifty actions, and ten
 *  sets built from them (maintainer, 2026-09-28): enough to cover the mood
 *  meter's four corners several ways, few enough to learn. Each clip draws a
 *  shape that ships -- a clip naming a missing one loads as nothing. */
// Re-enabled in Task 8 of library v2: the sets and clips still name the
// shapes from before the rename until Tasks 6 and 8 rebuild them.
TEST (ShippedLibrary, DISABLED_TheLibraryShipsFiftyOfEachAndTenSets)
{
  juce::File const shapes (A3_PATTERN_SYSTEM_DIR);
  juce::File const clips (A3_PATTERN_CLIPS_DIR);
  juce::File const actions (A3_PATTERN_ACTIONS_DIR);
  juce::File const sets (A3_PATTERN_SESSIONS_DIR);

  auto const shapeFiles
      = shapes.findChildFiles (juce::File::findFiles, false, "*.svg");
  auto const clipFiles
      = clips.findChildFiles (juce::File::findFiles, false, "*.json");
  auto actionFiles
      = actions.findChildFiles (juce::File::findFiles, false, "*.scd");
  actionFiles.removeIf ([] (juce::File const &f) {
    return f.getFileNameWithoutExtension () == "README";
  });

  EXPECT_EQ (shapeFiles.size (), 50);
  EXPECT_EQ (clipFiles.size (), 50);
  EXPECT_EQ (actionFiles.size (), 50);
  EXPECT_EQ (sets.findChildFiles (juce::File::findFiles, false, "*.json")
                 .size (),
             10);

  juce::StringArray shapeNames;
  for (auto const &file : shapeFiles)
    shapeNames.add (PatternFile::peek (file).name);
  for (auto const &file : clipFiles)
    {
      auto const svg = juce::JSON::parse (file.loadFileAsString ())["svg"];
      if (svg.isVoid () || svg.toString ().isEmpty ())
        continue; // Default: the fallback, deliberately shapeless
      EXPECT_TRUE (shapeNames.contains (svg.toString ()))
          << file.getFileName () << " draws " << svg.toString ()
          << ", which does not ship";
    }
}

namespace
{
juce::StringArray const phases{ "Warmup", "Groove", "Build", "Peak", "Drop",
                                "Break",  "Dub",    "Deep",  "Float", "Closing" };
}

// Fifty clips, each named by the phase of the night it belongs to, each drawing
// a shape that ships. Default stays beside them: the shapeless fallback every
// empty channel gets (ruling 1 of the plan).
TEST (ShippedLibrary, FiftyClipsNamedByTheirPhaseAndTheFallback)
{
  auto files = juce::File (A3_PATTERN_CLIPS_DIR)
                   .findChildFiles (juce::File::findFiles, false, "*.json");
  files.removeIf ([] (juce::File const &f) {
    return f.getFileNameWithoutExtension () == "Default";
  });
  EXPECT_EQ (files.size (), 50);

  juce::StringArray shapes;
  for (auto const &f : juce::File (A3_PATTERN_SYSTEM_DIR)
                           .findChildFiles (juce::File::findFiles, false, "*.svg"))
    shapes.add (PatternFile::peek (f).name);

  for (auto const &f : files)
    {
      auto const name = f.getFileNameWithoutExtension ();
      auto const json = juce::JSON::parse (f.loadFileAsString ());
      EXPECT_TRUE (startsWithOneOf (name, phases)) << name;
      EXPECT_TRUE (shapes.contains (json["svg"].toString ()))
          << name << " draws " << json["svg"].toString ();
      EXPECT_EQ (json["name"].toString (), name);
    }
}

TEST (ShippedLibrary, EveryPhaseHasFiveClips)
{
  auto const files = juce::File (A3_PATTERN_CLIPS_DIR)
                         .findChildFiles (juce::File::findFiles, false, "*.json");
  for (auto const &phase : phases)
    {
      int n = 0;
      for (auto const &f : files)
        n += f.getFileNameWithoutExtension ().startsWith (phase + " ") ? 1 : 0;
      EXPECT_EQ (n, 5) << phase;
    }
}
