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

#include "ClipSettingsFields.hh"
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
  turned.squeezeX = -0.5f;
  turned.squeezeY = 0.25f;
  turned.endAction = EndAction::Pause;
  set.channels[0].slots[0].overrides = turned;

  auto const file = tempSession ("a3-session-overrides.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const read = loadSession (file, 1, 1);
  ASSERT_TRUE (read.channels[0].slots[0].overrides.has_value ());
  EXPECT_EQ (read.channels[0].slots[0].overrides->spin, 4);
  EXPECT_FLOAT_EQ (read.channels[0].slots[0].overrides->rotate, 0.5f);
  EXPECT_FLOAT_EQ (read.channels[0].slots[0].overrides->squeezeX, -0.5f);
  EXPECT_FLOAT_EQ (read.channels[0].slots[0].overrides->squeezeY, 0.25f);
  EXPECT_EQ (read.channels[0].slots[0].overrides->endAction,
             EndAction::Pause);

  file.deleteFile ();
}

// A slot names the two files it came from -- the clip its values are from and
// the action ACT fires -- so a set brings back what was in front of you rather
// than a shape with the settings guessed at. By name and without a path: a set
// is carried between machines, and a path names a folder that is not there on
// the next one.
TEST (SessionFile, ASlotCarriesItsClipAndItsAction)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].slots[0].patternName = "Wave";
  set.channels[0].slots[0].clipFile = "Slow Turn";
  set.channels[0].slots[0].action = "Slam";

  auto const file = tempSession ("a3-session-references.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const text = file.loadFileAsString ();
  EXPECT_FALSE (text.contains ("/"))
      << "a path would name a folder the next stick does not have";

  auto const read = loadSession (file, 1, 1);
  EXPECT_EQ (read.channels[0].slots[0].clipFile, "Slow Turn");
  EXPECT_EQ (read.channels[0].slots[0].action, "Slam");

  file.deleteFile ();
}

// Whether a slot was running comes back with it, so loading a set means the
// room sounds the way it did rather than silent until eight pads are pressed.
TEST (SessionFile, ASlotRemembersWhetherItWasRunning)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (2);
  set.channels[0].slots[0].patternName = "Wave";
  set.channels[0].slots[0].playing = true;
  set.channels[0].slots[1].patternName = "Circle";

  auto const file = tempSession ("a3-session-playing.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const read = loadSession (file, 1, 2);
  EXPECT_TRUE (read.channels[0].slots[0].playing);
  EXPECT_FALSE (read.channels[0].slots[1].playing);

  file.deleteFile ();
}

// Whether, and nothing more. Where it had got to is not in the format at all:
// a set coming back mid-figure would start somewhere nobody chose.
TEST (SessionFile, WhereAClipHadGotToIsNotInTheSet)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].slots[0].patternName = "Wave";
  set.channels[0].slots[0].playing = true;

  auto const file = tempSession ("a3-session-no-position.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const text = file.loadFileAsString ().toLowerCase ();
  EXPECT_FALSE (text.contains ("position"));
  EXPECT_FALSE (text.contains ("phase"));

  file.deleteFile ();
}

// A slot with neither writes neither. A slot filled straight from a shape has
// no clip file and fires no action, and a file full of empty strings says that
// twice as loudly as leaving them out.
TEST (SessionFile, ASlotWithNothingBehindItSaysNothingAboutIt)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].slots[0].patternName = "Wave";

  auto const file = tempSession ("a3-session-no-references.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const text = file.loadFileAsString ();
  EXPECT_FALSE (text.contains ("\"clip\""));
  EXPECT_FALSE (text.contains ("\"action\""));
  EXPECT_FALSE (text.contains ("\"playing\""));

  auto const read = loadSession (file, 1, 1);
  EXPECT_TRUE (read.channels[0].slots[0].clipFile.empty ());
  EXPECT_TRUE (read.channels[0].slots[0].action.empty ());

  file.deleteFile ();
}

// Every field a clip carries has to survive being written into a set as an
// override, or a set quietly loses whatever it forgot to name -- and a set is
// where a clip's deviations live. Walked off the shared list for the same
// reason the clip file's test is. See ClipSettingsFields.hh.
TEST (SessionFile, EveryFieldASettingHasSurvivesAnOverride)
{
  auto const file = tempSession ("a3-session-every-field.json");

  for (auto const &[name, mutate] : clipSettingsFields ())
    {
      Session set;
      set.channels.resize (1);
      set.channels[0].slots.resize (1);
      set.channels[0].slots[0].patternName = "Wave";

      ClipSettings turned;
      mutate (turned);
      set.channels[0].slots[0].overrides = turned;

      ASSERT_TRUE (saveSession (file, set)) << name;

      auto const read = loadSession (file, 1, 1);
      ASSERT_TRUE (read.channels[0].slots[0].overrides.has_value ()) << name;
      EXPECT_EQ (*read.channels[0].slots[0].overrides, turned)
          << name << " did not survive the set";
    }

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
TEST (SessionFile, EveryShippedSetIsOneClipAndSixActionsPerChannel)
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

// ── The speed keys travel with the set ────────────────────────────────────

// What the four keys carry is how a set is played -- a slow set wants its
// slow speeds under the finger -- so loading one puts them back.
TEST (SessionFile, TheSpeedKeysSurviveTheRoundTrip)
{
  auto set = aSet ();
  set.speedButtonLog2 = std::array<int, numSpeedButtons>{ 1, -2, -5, -7 };

  auto const file = tempSession ("a3-session-speeds.json");
  ASSERT_TRUE (saveSession (file, set));

  auto const read = loadSession (file, numChannels, numSlots);
  ASSERT_TRUE (read.speedButtonLog2.has_value ());
  EXPECT_EQ (*read.speedButtonLog2, *set.speedButtonLog2);

  file.deleteFile ();
}

// A set written before the keys were part of it says nothing about them, and
// that has to go on meaning "leave the device's keys alone" -- not "put the
// defaults back".
TEST (SessionFile, AnOlderSessionLeavesTheSpeedKeysAlone)
{
  auto const file = tempSession ("a3-session-no-speeds.json");
  file.replaceWithText (
      R"({"channels":[{"threeD":0.5,"freq":0.0,"q":0.0,)"
      R"("slots":[{"pattern":"Wave","recordLengthLog2":0}]}]})");

  EXPECT_FALSE (loadSession (file, 1, 1).speedButtonLog2.has_value ());

  file.deleteFile ();
}

// A hand-edited file with the wrong number of keys is not half-applied.
TEST (SessionFile, AWrongNumberOfSpeedKeysIsIgnored)
{
  auto const file = tempSession ("a3-session-bad-speeds.json");
  file.replaceWithText (R"({"channels":[],"speedKeys":[0,-3]})");

  EXPECT_FALSE (loadSession (file, 1, 1).speedButtonLog2.has_value ());

  file.deleteFile ();
}

// ── One clip per channel, six actions (2026-09-27) ───────────────────────

// Six action buttons per channel, each by the script's name (a set travels)
// and with how it is played -- written only where that differs from the
// defaults, like a slot's overrides.
TEST (SessionFile, AChannelCarriesSixActionsWithTheirFeel)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  auto &actions = set.channels[0].actions;
  actions[0].script = "Bloom";
  ActionFeel feel;
  feel.envelopeAttack = 5;
  feel.actMode = ActMode::Hold;
  actions[0].feel = feel;
  actions[3].script = "Ground";

  auto const file = tempSet ("a3-session-actions.json");
  ASSERT_TRUE (saveSession (file, set));
  auto const read = loadSession (file, 1, 1);

  EXPECT_EQ (read.channels[0].actions[0].script, "Bloom");
  ASSERT_TRUE (read.channels[0].actions[0].feel.has_value ());
  EXPECT_EQ (*read.channels[0].actions[0].feel, feel);
  EXPECT_EQ (read.channels[0].actions[3].script, "Ground");
  EXPECT_FALSE (read.channels[0].actions[3].feel.has_value ());
  EXPECT_TRUE (read.channels[0].actions[1].script.empty ());

  // "slots" stays, so an older build still reads a new set.
  EXPECT_TRUE (file.loadFileAsString ().contains ("\"slots\""));
  file.deleteFile ();
}

// An old set had an action per slot, two per channel. Read by this build it
// keeps the clip of slot 1 -- and both actions, as A1 and A2, with the
// envelope each slot had been turned to.
TEST (SessionFile, AnOldSetsTwoSlotActionsBecomeA1AndA2)
{
  auto const file = tempSet ("a3-session-two-slots.json");
  ASSERT_TRUE (file.replaceWithText (R"({ "channels": [ { "slots": [
      { "pattern": "Wave", "action": "Slam",
        "overrides": { "envAttack": 5, "actMode": "hold" } },
      { "pattern": "Arc", "action": "Bloom" } ] } ] })"));

  auto const read = loadSession (file, 1, 1);
  ASSERT_EQ (read.channels[0].slots.size (), 1u);
  EXPECT_EQ (read.channels[0].slots[0].patternName, "Wave");

  auto const &actions = read.channels[0].actions;
  EXPECT_EQ (actions[0].script, "Slam");
  ASSERT_TRUE (actions[0].feel.has_value ());
  EXPECT_EQ (actions[0].feel->envelopeAttack, 5);
  EXPECT_EQ (actions[0].feel->actMode, ActMode::Hold);
  EXPECT_EQ (actions[1].script, "Bloom");
  EXPECT_FALSE (actions[1].feel.has_value ()) << "nothing turned: the script's";
  file.deleteFile ();
}

// The first start of the one-clip build copies every two-slot set aside
// before anything writes one back with one slot -- once, deleting nothing.
TEST (SessionFile, TwoSlotSetsAreCopiedAsideOnce)
{
  auto const root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-two-slot-backup");
  root.deleteRecursively ();
  ASSERT_TRUE (root.getChildFile ("sessions/user").createDirectory ());
  ASSERT_TRUE (root.getChildFile ("current.json").replaceWithText ("{}"));
  ASSERT_TRUE (root.getChildFile ("sessions/user/Mine.json").replaceWithText ("{}"));

  EXPECT_TRUE (migrateTwoSlotSets (root));
  auto const backup = root.getChildFile ("backup-two-slots");
  EXPECT_TRUE (backup.getChildFile ("current.json").existsAsFile ());
  EXPECT_TRUE (backup.getChildFile ("sessions/user/Mine.json").existsAsFile ());
  EXPECT_TRUE (root.getChildFile ("current.json").existsAsFile ())
      << "a copy, not a move";

  EXPECT_FALSE (migrateTwoSlotSets (root)) << "once";
  root.deleteRecursively ();
}

// A set with no action on any button says nothing about actions, so the
// slots' own still reach A1 and A2 when it is read -- an empty list written
// by a build that had not filled the buttons yet would otherwise wipe them.
TEST (SessionFile, NoActionsWrittenLeavesTheSlotsActionsToBeRead)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].slots[0].action = "Slam";

  auto const file = tempSet ("a3-session-no-actions.json");
  ASSERT_TRUE (saveSession (file, set));
  EXPECT_FALSE (file.loadFileAsString ().contains ("\"actions\""));
  EXPECT_EQ (loadSession (file, 1, 1).channels[0].actions[0].script, "Slam");
  file.deleteFile ();
}

/** The library ships fifty shapes, fifty clips and fifty actions, and ten
 *  sets built from them (maintainer, 2026-09-28): enough to cover the mood
 *  meter's four corners several ways, few enough to learn. Each clip draws a
 *  shape that ships -- a clip naming a missing one loads as nothing. */
TEST (SessionFile, TheLibraryShipsFiftyOfEachAndTenSets)
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

// ── What a button puts on the clip (2026-09-28) ──────────────────────────

// The values a button was turned to on the MOTION tile, per action entry --
// including one turned to a default, which is still the button's choice.
TEST (SessionFile, AnActionCarriesTheMotionItWasTurnedTo)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  auto &action = set.channels[0].actions[2];
  action.script = "Bloom";
  action.motion.set (MotionParam::Spin, 0.f);
  action.motion.set (MotionParam::Reach, -0.5f);
  action.motion.set (MotionParam::Elevation, 0.75f);
  action.motion.set (MotionParam::Speed, -2.f);
  action.motion.set (MotionParam::Direction,
                     static_cast<float> (PlayDirection::Forward));
  action.motion.set (MotionParam::EndAction,
                     static_cast<float> (EndAction::Pause));

  auto const file = tempSet ("a3-session-action-motion.json");
  ASSERT_TRUE (saveSession (file, set));
  auto const read = loadSession (file, 1, 1);

  EXPECT_EQ (read.channels[0].actions[2].motion, action.motion);
  EXPECT_TRUE (read.channels[0].actions[0].motion.empty ());
  EXPECT_TRUE (file.loadFileAsString ().contains ("\"motion\""));
  file.deleteFile ();
}

// Nothing turned, nothing written: a set stays as short as it was.
TEST (SessionFile, NoMotionIsWrittenWhereNothingWasTurned)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].actions[0].script = "Bloom";

  auto const file = tempSet ("a3-session-no-motion.json");
  ASSERT_TRUE (saveSession (file, set));
  EXPECT_FALSE (file.loadFileAsString ().contains ("\"motion\""));
  file.deleteFile ();
}

// A set from before the tile loads as it did: every value from the script.
TEST (SessionFile, AnOlderActionEntryLoadsWithNothingTurned)
{
  auto const file = tempSet ("a3-session-before-motion.json");
  ASSERT_TRUE (file.replaceWithText (R"({ "channels": [ { "slots": [ {} ],
      "actions": [ { "script": "Slam", "feel": { "envAttack": 5 } } ] } ] })"));

  auto const read = loadSession (file, 1, 1);
  auto const &action = read.channels[0].actions[0];
  EXPECT_EQ (action.script, "Slam");
  ASSERT_TRUE (action.feel.has_value ());
  EXPECT_EQ (action.feel->envelopeAttack, 5);
  EXPECT_TRUE (action.motion.empty ());
  file.deleteFile ();
}

// The words a slot's overrides use, so one file has one name per value.
TEST (SessionFile, MotionIsWrittenInTheSetsOwnWords)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  auto &action = set.channels[0].actions[0];
  action.script = "Bloom";
  action.motion.set (MotionParam::Direction,
                     static_cast<float> (PlayDirection::Reverse));
  action.motion.set (MotionParam::Swell, 3.f);

  auto const file = tempSet ("a3-session-motion-words.json");
  ASSERT_TRUE (saveSession (file, set));
  auto const text = file.loadFileAsString ();
  EXPECT_TRUE (text.contains ("\"direction\": \"rev\"")) << text;
  EXPECT_TRUE (text.contains ("\"swell\": 3")) << text;
  file.deleteFile ();
}

// What a button fires when its accent is over -- "then A3" -- per action
// entry, written only when set.
TEST (SessionFile, AnActionCarriesWhatComesAfterIt)
{
  Session set;
  set.channels.resize (1);
  set.channels[0].slots.resize (1);
  set.channels[0].actions[0].script = "Bloom";
  set.channels[0].actions[0].after = 2;
  set.channels[0].actions[1].script = "Slam";

  auto const file = tempSet ("a3-session-after.json");
  ASSERT_TRUE (saveSession (file, set));
  auto const text = file.loadFileAsString ();
  EXPECT_TRUE (text.contains ("\"after\": \"A3\"")) << text;
  EXPECT_EQ (text.indexOf ("\"after\""), text.lastIndexOf ("\"after\""))
      << "written for the one button that has it";

  auto const read = loadSession (file, 1, 1);
  EXPECT_EQ (read.channels[0].actions[0].after, std::optional<int> (2));
  EXPECT_FALSE (read.channels[0].actions[1].after.has_value ());
  file.deleteFile ();
}

TEST (SessionFile, AnOlderActionEntryHasNothingAfterIt)
{
  auto const file = tempSet ("a3-session-before-after.json");
  ASSERT_TRUE (file.replaceWithText (R"({ "channels": [ { "slots": [ {} ],
      "actions": [ { "script": "Slam" } ] } ] })"));
  auto const read = loadSession (file, 1, 1);
  EXPECT_FALSE (read.channels[0].actions[0].after.has_value ());
  file.deleteFile ();
}
