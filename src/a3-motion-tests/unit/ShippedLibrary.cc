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

#include <cmath>
#include <vector>

#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/PatternFile.hh>
#include <a3-motion-engine/PlaybackRate.hh>
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-ui/components/ActionEditing.hh>
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
TEST (ShippedLibrary, EveryShippedSetNamesShapesThatExist)
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
TEST (ShippedLibrary, EveryShippedSetIsOneClipAndSixActionsPerChannel)
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

/** The library ships fifty shapes and, since 2026-10-08, fifteen sets: the
 *  ten of 2026-09-28, four moods built like Groove (Tribal, Acid, Ambient,
 *  Tension) and Space, the motion jockey's set -- five clips per phase, so
 *  seventy-five, and seventy-seven actions.
 *  Each clip draws a shape that ships -- a clip naming a missing one loads as
 *  nothing. */
TEST (ShippedLibrary, TheLibraryShipsFiftyShapesAndFifteenSets)
{
  juce::File const shapes (A3_PATTERN_SYSTEM_DIR);
  juce::File const clips (A3_PATTERN_CLIPS_DIR);
  juce::File const actions (A3_PATTERN_ACTIONS_DIR);
  juce::File const sets (A3_PATTERN_SESSIONS_DIR);

  auto const shapeFiles
      = shapes.findChildFiles (juce::File::findFiles, false, "*.svg");
  // Default stays beside the fifty: the shapeless fallback (ruling 1 of the
  // library v2 plan).
  auto clipFiles
      = clips.findChildFiles (juce::File::findFiles, false, "*.json");
  clipFiles.removeIf ([] (juce::File const &f) {
    return f.getFileNameWithoutExtension () == "Default";
  });
  auto actionFiles
      = actions.findChildFiles (juce::File::findFiles, false, "*.scd");
  actionFiles.removeIf ([] (juce::File const &f) {
    return f.getFileNameWithoutExtension () == "README";
  });

  EXPECT_EQ (shapeFiles.size (), 50);
  EXPECT_EQ (clipFiles.size (), 75);
  EXPECT_EQ (actionFiles.size (), 77);
  EXPECT_EQ (sets.findChildFiles (juce::File::findFiles, false, "*.json")
                 .size (),
             15);

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
juce::StringArray const phases{ "Warmup", "Groove", "Build",   "Peak",
                                "Drop",   "Break",  "Dub",     "Deep",
                                "Float",  "Closing", "Tribal", "Acid",
                                "Ambient", "Tension", "Space" };
}

// Seventy-five clips, each named by the phase of the night it belongs to, each
// drawing a shape that ships. Default stays beside them: the shapeless
// fallback every empty channel gets (ruling 1 of the plan).
TEST (ShippedLibrary, SeventyFiveClipsNamedByTheirPhaseAndTheFallback)
{
  auto files = juce::File (A3_PATTERN_CLIPS_DIR)
                   .findChildFiles (juce::File::findFiles, false, "*.json");
  files.removeIf ([] (juce::File const &f) {
    return f.getFileNameWithoutExtension () == "Default";
  });
  EXPECT_EQ (files.size (), 75);

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

namespace
{
juce::StringArray const kinds{ "Move", "Lift", "Width", "Speed", "Dub", "FX", "Cue" };

juce::Array<juce::File>
shippedActionFiles ()
{
  auto files = juce::File (A3_PATTERN_ACTIONS_DIR)
                   .findChildFiles (juce::File::findFiles, false, "*.scd");
  files.removeIf ([] (juce::File const &f) {
    return f.getFileNameWithoutExtension () == "README";
  });
  return files;
}
}

// Seventy-seven actions, each named by what it does.
TEST (ShippedLibrary, SeventySevenActionsNamedByWhatTheyDo)
{
  auto const files = shippedActionFiles ();
  EXPECT_EQ (files.size (), 77);
  for (auto const &f : files)
    EXPECT_TRUE (startsWithOneOf (f.getFileNameWithoutExtension (), kinds))
        << f.getFileName ();
}

// Every FX changes the sound. Since 2026-10-03 any other action may too: the
// maintainer's own gestures move and sound at once (Speed Half with a filter
// envelope), and the old rule kept exactly those out of the library.
// Resolved against a clip whose ceilings are all up, so a ceiling left
// commented out would show.
TEST (ShippedLibrary, EveryFXChangesTheSound)
{
  ClipSettings loud;
  loud.envelopeMax = 1.f;
  loud.freqMax = 1.f;
  loud.qMax = 1.f;

  for (auto const &f : shippedActionFiles ())
    {
      auto const name = f.getFileNameWithoutExtension ();
      auto const r = runActionScript (f.loadFileAsString (), loud, 1);
      EXPECT_TRUE (r.errors.isEmpty ())
          << name << ": " << r.errors.joinIntoString (" | ");
      if (!name.startsWith ("FX "))
        continue;
      auto const silent = r.settings.envelopeMax == 0.f
                          && r.settings.freqMax == 0.f
                          && r.settings.qMax == 0.f;
      EXPECT_FALSE (silent) << name << " is FX but leaves the sound alone";
    }
}

// A Cue is named after the clip it puts on the channel, and that clip ships.
TEST (ShippedLibrary, EveryCueNamesAClipThatShipsAndNothingElse)
{
  auto const clips = juce::File (A3_PATTERN_CLIPS_DIR).getParentDirectory ();
  int cues = 0;
  for (auto const &f : shippedActionFiles ())
    {
      auto const name = f.getFileNameWithoutExtension ();
      auto const r = runActionScript (f.loadFileAsString (), ClipSettings{}, 1);
      if (!name.startsWith ("Cue "))
        {
          EXPECT_FALSE (r.clip.has_value ()) << name;
          continue;
        }
      ++cues;
      ASSERT_TRUE (r.clip.has_value ()) << name;
      EXPECT_EQ ("Cue " + *r.clip, name) << "a Cue is named after its clip";
      EXPECT_TRUE (cueClipFor (r.clip, clips).error.isEmpty ()) << name;
    }
  EXPECT_EQ (cues, 14);
}

// Fifteen sets, one for each phase of the night, laid out the same way: the four
// clips carry the set's own phase, A5 is the FX, A6 the Cue into what comes
// next, and A1..A4 only move.
TEST (ShippedLibrary, FifteenSetsOnePerPhaseLaidOutTheSameWay)
{
  auto const files = juce::File (A3_PATTERN_SESSIONS_DIR)
                         .findChildFiles (juce::File::findFiles, false, "*.json");
  EXPECT_EQ (files.size (), 15);
  for (auto const &f : files)
    {
      auto const set = loadSession (f, 4, 1);
      auto const name = juce::String (set.name);
      EXPECT_TRUE (phases.contains (name)) << name;
      for (auto const &channel : set.channels)
        {
          EXPECT_TRUE (juce::String (channel.slots[0].clipFile)
                           .startsWith (name + " "))
              << name << " holds " << channel.slots[0].clipFile;
          auto const script = [&] (int b) {
            return juce::String (channel.actions[static_cast<size_t> (b)].script);
          };
          EXPECT_TRUE (script (4).startsWith ("FX ")) << name << " A5 " << script (4);
          EXPECT_TRUE (script (5).startsWith ("Cue ")) << name << " A6 " << script (5);
          for (int b = 0; b < 4; ++b)
            EXPECT_FALSE (script (b).startsWith ("FX ")
                          || script (b).startsWith ("Cue "))
                << name << " A" << (b + 1) << " " << script (b);
        }
    }
}

// ── Space: the motion jockey's set (2026-10-08) ─────────────────────────
//
// Built on .claude/notes/auditory-motion-research.md: motion a room can
// actually hear. These are the numbers that note derives, held here so a
// later edit to the set cannot quietly drift past them.

namespace
{
juce::String const spaceSet = "Space";

float
shapeBeats (juce::String const &shapeName)
{
  for (auto const &f : juce::File (A3_PATTERN_SYSTEM_DIR)
                           .findChildFiles (juce::File::findFiles, false, "*.svg"))
    {
      auto const peeked = PatternFile::peek (f);
      if (juce::String (peeked.name) == shapeName)
        return static_cast<float> (peeked.lengthBeats);
    }
  return 0.f;
}

/** One lap of the clip's own figure, in beats. */
float
lapBeats (Clip const &clip)
{
  return playbackLengthBeats (shapeBeats (clip.svg), clip.settings.speedLog2);
}

bool
isPowerOfTwo (float beats)
{
  auto const exponent = std::log2 (beats);
  return beats > 0.f && std::abs (exponent - std::round (exponent)) < 1e-4f;
}

/** How far apart two standing angles are, in revolutions, the short way. */
float
turnBetween (float a, float b)
{
  auto const d = std::fmod (std::abs (a - b), 1.f);
  return std::min (d, 1.f - d);
}

std::vector<Clip>
spaceClips ()
{
  std::vector<Clip> clips;
  for (auto const &f : juce::File (A3_PATTERN_CLIPS_DIR)
                           .findChildFiles (juce::File::findFiles, false, "*.json"))
    if (f.getFileNameWithoutExtension ().startsWith (spaceSet + " "))
      if (auto clip = ClipFile::load (f))
        clips.push_back (*clip);
  return clips;
}
}

/** Every lap a whole power of two of beats, between two bars (90 deg/s at
 *  120 BPM, far under the 900 deg/s where direction is lost -- Feron 2010)
 *  and sixteen; the spin adds at most a turn per four bars; and a sweep of
 *  the height is at most half as fast as the lap, because the ear follows
 *  vertical movement slower than horizontal (Saberi & Perrott 1990). No
 *  squeeze sweeps: one movement per clip, not three at once. */
TEST (ShippedLibrary, SpaceClipsMoveAtSpeedsARoomCanFollow)
{
  auto const clips = spaceClips ();
  ASSERT_EQ (clips.size (), 5u);

  for (auto const &clip : clips)
    {
      auto const lap = lapBeats (clip);
      auto const &s = clip.settings;
      EXPECT_TRUE (isPowerOfTwo (lap)) << clip.name << ": " << lap;
      EXPECT_GE (lap, 8.f) << clip.name;
      EXPECT_LE (lap, 64.f) << clip.name;
      EXPECT_LE (std::abs (s.spin), 4) << clip.name;
      EXPECT_EQ (s.squeezeXLfo, 0) << clip.name;
      EXPECT_EQ (s.squeezeYLfo, 0) << clip.name;
      EXPECT_EQ (s.tiltLfo, 0) << clip.name;
      EXPECT_EQ (s.rollLfo, 0) << clip.name;
      for (auto const sweep : { s.elevationLfo, s.reachLfo })
        if (sweep != 0)
          EXPECT_GE (lfoBarsPerCycle (sweep) * 4.f, 2.f * lap)
              << clip.name << ": the height moves faster than half the lap";
    }
}

/** Few things moving at once (4DSOUND, "less becomes far more"): at most two
 *  channels carry a clear 3d, and the anchor -- the deck with the bassline --
 *  barely moves, because below ~150 Hz a club hears no direction at all. */
TEST (ShippedLibrary, SpaceMovesTwoThingsAndKeepsAnAnchor)
{
  auto const file = juce::File (A3_PATTERN_SESSIONS_DIR)
                        .getChildFile (spaceSet + ".json");
  ASSERT_TRUE (file.existsAsFile ());
  auto const set = loadSession (file, 4, 1);

  int clear = 0;
  bool anchored = false;
  for (auto const &channel : set.channels)
    {
      clear += channel.threeD >= 0.3f ? 1 : 0;
      if (juce::String (channel.slots[0].clipFile) == "Space Anchor")
        {
          anchored = true;
          EXPECT_LE (channel.threeD, 0.2f);
        }
    }
  EXPECT_LE (clear, 2);
  EXPECT_TRUE (anchored);

  auto const anchor = ClipFile::load (
      juce::File (A3_PATTERN_CLIPS_DIR).getChildFile ("Space Anchor.json"));
  ASSERT_TRUE (anchor.has_value ());
  EXPECT_GE (lapBeats (*anchor), 64.f);
  EXPECT_LE (std::abs (anchor->settings.reach), 0.2f);
  EXPECT_EQ (anchor->settings.spin, 0);
}

/** What each of Space's buttons does to each of its clips stays audible and
 *  stays a path: a turn of the figure is at least 30 degrees (the smallest
 *  bend a crowd hears off-centre, Grantham 1986); a lap never shorter than
 *  two beats (past that it is a texture); a move of the height at least 20
 *  degrees and with the band brightened, because elevation is carried by
 *  6-12 kHz (Langendijk & Bronkhorst 2002); and no button narrows the band --
 *  under an octave a moving band stops moving (Yost & Zhong 2014). */
TEST (ShippedLibrary, SpaceButtonsMakeChangesARoomCanHear)
{
  auto const file = juce::File (A3_PATTERN_SESSIONS_DIR)
                        .getChildFile (spaceSet + ".json");
  ASSERT_TRUE (file.existsAsFile ());
  auto const set = loadSession (file, 4, 1);
  auto const actions = juce::File (A3_PATTERN_ACTIONS_DIR);

  for (auto const &clip : spaceClips ())
    for (auto const &button : set.channels[0].actions)
      {
        auto const name = juce::String (button.script);
        if (name.startsWith ("Cue "))
          continue;
        auto const r = runActionScript (
            actions.getChildFile (name + ".scd").loadFileAsString (),
            clip.settings, 1);
        ASSERT_TRUE (r.errors.isEmpty ()) << name;
        auto const &before = clip.settings;
        auto const &after = r.settings;
        auto const label = name + " on " + juce::String (clip.name);

        if (after.rotate != before.rotate)
          EXPECT_GE (turnBetween (after.rotate, before.rotate), 1.f / 12.f)
              << label;
        EXPECT_GE (playbackLengthBeats (shapeBeats (clip.svg), after.speedLog2),
                   2.f)
            << label;
        EXPECT_LE (std::abs (after.spin), 6) << label;
        if (after.elevationBase != before.elevationBase)
          {
            EXPECT_GE (std::abs (after.elevationBase - before.elevationBase),
                       1.f / 9.f)
                << label;
            EXPECT_GT (after.freqMax, 0.f) << label << " lifts a dark band";
          }
        EXPECT_EQ (after.qMax, 0.f) << label << " narrows the band";
      }

  // The same six on every channel: the hands learn one panel.
  for (auto const &channel : set.channels)
    for (size_t b = 0; b < channel.actions.size (); ++b)
      EXPECT_EQ (juce::String (channel.actions[b].script),
                 juce::String (set.channels[0].actions[b].script));
}

// The buttons sit the same way in every phase set (a3-doc's library page):
// A1 and A3 move towards "more", A2 and A4 towards "less", as each script's
// own Mood: line says. Break and Float carried Width Breathe, a "less", on
// A3 (docs question 9, 2026-10-08). The four mood sets use A1..A4 for
// gestures of their own mood and are not held to it.
TEST (ShippedLibrary, PhaseSetsPutMoreLeftAndLessRight)
{
  juce::StringArray const moodSets{ "Tribal", "Tension", "Acid", "Ambient" };
  auto const moodOf = [] (juce::String const &script) {
    auto const file
        = juce::File (A3_PATTERN_ACTIONS_DIR).getChildFile (script + ".scd");
    for (auto const &line : juce::StringArray::fromLines (file.loadFileAsString ()))
      if (line.contains ("Mood:"))
        return line.fromFirstOccurrenceOf ("Mood:", false, false)
            .trim ()
            .upToFirstOccurrenceOf (" ", false, false);
    return juce::String{};
  };

  auto const files = juce::File (A3_PATTERN_SESSIONS_DIR)
                         .findChildFiles (juce::File::findFiles, false, "*.json");
  for (auto const &f : files)
    {
      auto const set = loadSession (f, 4, 1);
      auto const name = juce::String (set.name);
      if (moodSets.contains (name))
        continue;
      for (auto const &channel : set.channels)
        for (int b = 0; b < 4; ++b)
          {
            auto const script
                = juce::String (channel.actions[static_cast<size_t> (b)].script);
            EXPECT_EQ (moodOf (script), b % 2 == 0 ? "more" : "less")
                << name << " A" << (b + 1) << " " << script;
          }
    }
}

// The library the tests hold to their promises is what is committed, not what
// the rig's working copy holds: ACTION writes its settings into the scripts
// while a set is played (2026-09-29, Speed Double's ~envelopeMax 0.05 turned
// OnlyFXChangesTheSound red), and the skin panel writes into custom.json.
// test.sh exports HEAD into build/committed before every run.
TEST (ShippedLibrary, TheTestsReadTheCommittedState)
{
  for (auto const *path : { A3_PATTERN_ACTIONS_DIR, A3_PATTERN_CLIPS_DIR,
                            A3_PATTERN_SESSIONS_DIR, A3_PATTERN_SYSTEM_DIR,
                            A3_CONFIG_JSON_PATH })
    EXPECT_TRUE (juce::String (path).contains ("/committed/")) << path;
  EXPECT_TRUE (juce::File (A3_PATTERN_ACTIONS_DIR).isDirectory ())
      << "nothing exported -- run the suite through test.sh";
}
