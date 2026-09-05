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

#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-ui/SessionFile.hh>

using namespace a3;

namespace
{
constexpr int numChannels = 4;
constexpr int numSlots = 2;

juce::File
tempSet (juce::String const &name)
{
  return juce::File::getSpecialLocation (juce::File::tempDirectory)
      .getChildFile (name);
}

Session
aSet ()
{
  Session set;
  set.channels.resize (numChannels);

  for (int ch = 0; ch < numChannels; ++ch)
    {
      auto &channel = set.channels[static_cast<size_t> (ch)];
      channel.threeD = 0.1f * static_cast<float> (ch + 1);
      channel.freq = 0.2f * static_cast<float> (ch + 1);
      channel.q = 0.05f * static_cast<float> (ch + 1);
      channel.slots.resize (numSlots);

      for (int slot = 0; slot < numSlots; ++slot)
        {
          auto &s = channel.slots[static_cast<size_t> (slot)];
          s.patternName
              = "Rec_" + std::to_string (ch) + std::to_string (slot) + ".svg";
          s.recordLengthLog2 = ch - slot;
        }
    }

  return set;
}
}

// A set is a thing you carry to a gig, so it has to come back the same. Every
// field, because the last time settings were half-written nobody noticed until
// a clip sounded different.
TEST (SetFileTest, ASetSurvivesARoundTrip)
{
  auto const file = tempSet ("a3-set-roundtrip.json");
  auto const written = aSet ();

  ASSERT_TRUE (saveSession (file, written));
  auto const read = loadSession (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), written.channels.size ());
  for (size_t ch = 0; ch < read.channels.size (); ++ch)
    {
      auto const &a = written.channels[ch];
      auto const &b = read.channels[ch];

      EXPECT_FLOAT_EQ (b.threeD, a.threeD) << "channel " << ch;
      EXPECT_FLOAT_EQ (b.freq, a.freq) << "channel " << ch;
      EXPECT_FLOAT_EQ (b.q, a.q) << "channel " << ch;

      ASSERT_EQ (b.slots.size (), a.slots.size ());
      for (size_t slot = 0; slot < b.slots.size (); ++slot)
        {
          EXPECT_EQ (b.slots[slot].patternName, a.slots[slot].patternName);
          EXPECT_EQ (b.slots[slot].recordLengthLog2,
                     a.slots[slot].recordLengthLog2);
        }
    }

  file.deleteFile ();
}

// No set is not an error. A device somebody has not brought a set to is a
// device with empty slots, and it has to start rather than refuse to.
TEST (SetFileTest, AMissingSetIsAnEmptySet)
{
  auto const file = tempSet ("a3-set-does-not-exist.json");
  file.deleteFile ();

  auto const read = loadSession (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), static_cast<size_t> (numChannels));
  for (auto const &channel : read.channels)
    {
      ASSERT_EQ (channel.slots.size (), static_cast<size_t> (numSlots));
      for (auto const &slot : channel.slots)
        EXPECT_TRUE (slot.patternName.empty ());
    }
}

// Same for a file that is there but is not a set. A stick with a stray
// set.json on it should not stop the device coming up.
TEST (SetFileTest, RubbishInTheFileIsAnEmptySetToo)
{
  auto const file = tempSet ("a3-set-rubbish.json");
  file.replaceWithText ("this is not json {{{");

  auto const read = loadSession (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), static_cast<size_t> (numChannels));
  for (auto const &channel : read.channels)
    EXPECT_EQ (channel.slots.size (), static_cast<size_t> (numSlots));

  file.deleteFile ();
}

// A set written by a device with fewer channels or slots than this one has
// must still load, filling what it does not mention. Hardware outlives file
// formats.
TEST (SetFileTest, ASmallerSetFillsOutToTheDevicesShape)
{
  auto const file = tempSet ("a3-set-smaller.json");

  Session small;
  small.channels.resize (2);
  for (auto &channel : small.channels)
    {
      channel.slots.resize (1);
      channel.slots[0].patternName = "Only.svg";
    }
  ASSERT_TRUE (saveSession (file, small));

  auto const read = loadSession (file, numChannels, numSlots);

  ASSERT_EQ (read.channels.size (), static_cast<size_t> (numChannels));
  for (auto const &channel : read.channels)
    ASSERT_EQ (channel.slots.size (), static_cast<size_t> (numSlots));

  EXPECT_EQ (read.channels[0].slots[0].patternName, "Only.svg");
  EXPECT_TRUE (read.channels[0].slots[1].patternName.empty ());
  EXPECT_TRUE (read.channels[3].slots[0].patternName.empty ());

  file.deleteFile ();
}

// ── A name, and what each slot has been turned to ────────────────────────

namespace
{
juce::File
tempSession (juce::String const &name)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile (name);
  file.deleteFile ();
  return file;
}
}

TEST (SessionFile, ASessionKeepsItsName)
{
  Session set;
  set.name = "live-set-a";
  set.channels.resize (1);
  set.channels[0].slots.resize (1);

  auto const file = tempSession ("a3-session-name.json");
  ASSERT_TRUE (saveSession (file, set));

  EXPECT_EQ (loadSession (file, 1, 1).name, "live-set-a");

  file.deleteFile ();
}

// A session refers to clips rather than copying them, so two slots can hold
// the same clip -- and turning a control on one must not change the other.
// The difference lives in the session until somebody saves it into the clip.
TEST (SessionFile, WhatASlotHasBeenTurnedToSurvivesTheRoundTrip)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].slots[0].patternName = "Wave";

  ClipSettings turned;
  turned.spin = 4;
  turned.rotate = 0.5f;
  turned.endAction = EndAction::Bounce;
  set.channels[0].slots[0].overrides = turned;

  auto const file = tempSession ("a3-session-overrides.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const read = loadSession (file, 1, 1);
  ASSERT_TRUE (read.channels[0].slots[0].overrides.has_value ());
  EXPECT_EQ (read.channels[0].slots[0].overrides->spin, 4);
  EXPECT_FLOAT_EQ (read.channels[0].slots[0].overrides->rotate, 0.5f);
  EXPECT_EQ (read.channels[0].slots[0].overrides->endAction,
             EndAction::Bounce);

  file.deleteFile ();
}

// Only the fields that differ are written. A session carrying a whole second
// copy of every clip would drift away from the clips themselves without
// anyone noticing.
TEST (SessionFile, OnlyWhatDiffersIsWritten)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);

  ClipSettings turned;
  turned.spin = 4;
  set.channels[0].slots[0].overrides = turned;

  auto const file = tempSession ("a3-session-sparse.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const text = file.loadFileAsString ();
  EXPECT_TRUE (text.contains ("spin"));
  EXPECT_FALSE (text.contains ("reach"))
      << "an untouched field was written out anyway";
  EXPECT_FALSE (text.contains ("envAttack"));

  file.deleteFile ();
}

TEST (SessionFile, ASlotWithoutOverridesHasNone)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].slots[0].patternName = "Wave";

  auto const file = tempSession ("a3-session-plain.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const read = loadSession (file, 1, 1);
  EXPECT_FALSE (read.channels[0].slots[0].overrides.has_value ());

  file.deleteFile ();
}

// A session written before overrides existed loads as having none, rather
// than as having a set of defaults -- the two mean different things.
TEST (SessionFile, AnOlderSessionHasNoOverrides)
{
  auto const file = tempSession ("a3-session-old.json");
  file.replaceWithText (
      R"({"channels":[{"threeD":0.5,"freq":0.0,"q":0.0,)"
      R"("slots":[{"pattern":"Wave","recordLengthLog2":0}]}]})");

  auto const read = loadSession (file, 1, 1);
  EXPECT_EQ (read.channels[0].slots[0].patternName, "Wave");
  EXPECT_FALSE (read.channels[0].slots[0].overrides.has_value ());

  file.deleteFile ();
}

// ── The automatic set becomes a session like any other ───────────────────

TEST (SessionFile, TheOldAutomaticSetBecomesCurrent)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-session-migrate");
  root.deleteRecursively ();
  root.createDirectory ();

  auto const old = root.getChildFile ("set.json");
  old.replaceWithText (
      R"({"channels":[{"threeD":0.5,"freq":0.0,"q":0.0,)"
      R"("slots":[{"pattern":"Wave","recordLengthLog2":0}]}]})");

  EXPECT_TRUE (migrateSetToCurrent (root));

  auto const current = loadSession (root.getChildFile ("current.json"), 1, 1);
  EXPECT_EQ (current.channels[0].slots[0].patternName, "Wave");

  // Nothing deleted: a migration that leaves the old file can be run again.
  EXPECT_TRUE (old.existsAsFile ());

  root.deleteRecursively ();
}

// It runs on every start, so a second run must do nothing -- and a set.json
// dropped back in later, from somebody's backup, must not overwrite the
// session actually in use.
TEST (SessionFile, MigratingAgainLeavesTheSessionInUseAlone)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-session-migrate-twice");
  root.deleteRecursively ();
  root.createDirectory ();

  root.getChildFile ("set.json").replaceWithText (R"({"channels":[]})");
  ASSERT_TRUE (migrateSetToCurrent (root));

  root.getChildFile ("current.json")
      .replaceWithText (
          R"({"name":"mine","channels":[{"threeD":0.9,"freq":0.0,"q":0.0,)"
          R"("slots":[]}]})");

  EXPECT_FALSE (migrateSetToCurrent (root));
  EXPECT_EQ (loadSession (root.getChildFile ("current.json"), 1, 1).name,
             "mine");

  root.deleteRecursively ();
}

TEST (SessionFile, NothingToMigrateIsNotAnError)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-session-migrate-empty");
  root.deleteRecursively ();
  root.createDirectory ();

  EXPECT_FALSE (migrateSetToCurrent (root));

  root.deleteRecursively ();
}

/** A set names its takes rather than carrying them, so a shipped set is only
 *  as good as the names in it: rename a shape and every set pointing at it
 *  loads as an empty slot, silently, because a missing name is not an error
 *  here -- it is a slot nobody has filled. This is the one place that would
 *  notice. */
TEST (SessionFile, EveryShippedSetNamesShapesThatExist)
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
      auto const set = loadSession (file, 4, 2);
      EXPECT_EQ (juce::String (set.name), file.getFileNameWithoutExtension ())
          << "a set's name is what the browser lists it under";
      ASSERT_EQ (set.channels.size (), 4u) << file.getFileName ();

      for (auto const &channel : set.channels)
        {
          ASSERT_EQ (channel.slots.size (), 2u) << file.getFileName ();
          for (auto const &slot : channel.slots)
            EXPECT_TRUE (known.contains (juce::String (slot.patternName)))
                << file.getFileName () << ": no shape called "
                << slot.patternName;
        }
    }
}
