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
#include <a3-motion-engine/Pattern.hh>

using namespace a3;

namespace
{
juce::File
tempClip (juce::String const &name)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile (name);
  file.deleteFile ();
  return file;
}
}

TEST (ClipFile, AClipSurvivesARoundTrip)
{
  Clip clip;
  clip.name = "Wave slow";
  clip.aka = { "Wave 2" };
  clip.svg = "16_Wave";
  clip.settings.speedLog2 = -1;
  clip.settings.rotate = 0.25f;
  clip.settings.reach = 0.4f;
  clip.settings.clipTop = 0.1f;
  clip.settings.clipBottom = 0.2f;
  clip.settings.mirrorSouth = true;
  clip.settings.flat = true;
  clip.settings.flatElevation = 0.3f;
  clip.settings.spin = 2;
  clip.settings.reachLfo = -3;
  clip.settings.envelopeAttack = 1;
  clip.settings.envelopeDecay = 4;
  clip.settings.envelopeMax = 0.75f;
  clip.settings.actMode = ActMode::Hold;
  clip.settings.direction = PlayDirection::Reverse;
  clip.settings.endAction = EndAction::Bounce;
  clip.settings.fadeSixteenths = 8;

  auto const file = tempClip ("a3-clip-roundtrip.json");
  ASSERT_TRUE (ClipFile::save (clip, file));

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_EQ (read->name, "Wave slow");
  EXPECT_EQ (read->svg, "16_Wave");
  ASSERT_EQ (read->aka.size (), 1u);
  EXPECT_EQ (read->aka[0], "Wave 2");

  EXPECT_EQ (read->settings.speedLog2, -1);
  EXPECT_FLOAT_EQ (read->settings.rotate, 0.25f);
  EXPECT_FLOAT_EQ (read->settings.reach, 0.4f);
  EXPECT_FLOAT_EQ (read->settings.clipTop, 0.1f);
  EXPECT_FLOAT_EQ (read->settings.clipBottom, 0.2f);
  EXPECT_TRUE (read->settings.mirrorSouth);
  EXPECT_TRUE (read->settings.flat);
  EXPECT_FLOAT_EQ (read->settings.flatElevation, 0.3f);
  EXPECT_EQ (read->settings.spin, 2);
  EXPECT_EQ (read->settings.reachLfo, -3);
  EXPECT_EQ (read->settings.envelopeAttack, 1);
  EXPECT_EQ (read->settings.envelopeDecay, 4);
  EXPECT_FLOAT_EQ (read->settings.envelopeMax, 0.75f);
  EXPECT_EQ (read->settings.actMode, ActMode::Hold);
  EXPECT_EQ (read->settings.direction, PlayDirection::Reverse);
  EXPECT_EQ (read->settings.endAction, EndAction::Bounce);
  EXPECT_EQ (read->settings.fadeSixteenths, 8);

  file.deleteFile ();
}

// A file naming only what identifies it must load, with everything else at its
// default. That is what makes a hand-written clip file possible, and what
// stops a new setting from invalidating every file already on a stick.
TEST (ClipFile, AFileWithOnlyANameAndAShapeLoadsWithDefaults)
{
  auto const file = tempClip ("a3-clip-minimal.json");
  file.replaceWithText (R"({ "name": "Bare", "svg": "16_Circle" })");

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_EQ (read->name, "Bare");
  EXPECT_EQ (read->svg, "16_Circle");

  ClipSettings const defaults;
  EXPECT_EQ (read->settings.speedLog2, defaults.speedLog2);
  EXPECT_EQ (read->settings.endAction, defaults.endAction);
  EXPECT_EQ (read->settings.actMode, defaults.actMode);
  EXPECT_EQ (read->settings.fadeSixteenths, defaults.fadeSixteenths);
  EXPECT_FLOAT_EQ (read->settings.reach, defaults.reach);

  file.deleteFile ();
}

// Rubbish must not come back as a clip full of noise in the middle of a set:
// it fails to load, and the slot stays empty and says so.
TEST (ClipFile, RubbishDoesNotLoad)
{
  auto const file = tempClip ("a3-clip-rubbish.json");
  file.replaceWithText ("this is not json at all {{{");

  EXPECT_FALSE (ClipFile::load (file).has_value ());

  file.deleteFile ();
}

// A clip that names no shape plays nothing, so it is not a clip.
TEST (ClipFile, AClipWithoutAShapeDoesNotLoad)
{
  auto const file = tempClip ("a3-clip-shapeless.json");
  file.replaceWithText (R"({ "name": "Nowhere", "speed": -1 })");

  EXPECT_FALSE (ClipFile::load (file).has_value ());

  file.deleteFile ();
}

TEST (ClipFile, AMissingFileDoesNotLoad)
{
  EXPECT_FALSE (
      ClipFile::load (tempClip ("a3-clip-absent.json")).has_value ());
}

// The file has to be readable by a person on a laptop -- that is the whole
// reason names are the identity rather than opaque keys.
TEST (ClipFile, TheFileNamesItsValuesInWords)
{
  Clip clip;
  clip.name = "Readable";
  clip.svg = "16_Wave";
  clip.settings.endAction = EndAction::Bounce;
  clip.settings.direction = PlayDirection::Reverse;
  clip.settings.actMode = ActMode::Hold;

  auto const file = tempClip ("a3-clip-readable.json");
  ASSERT_TRUE (ClipFile::save (clip, file));

  auto const text = file.loadFileAsString ();
  EXPECT_TRUE (text.contains ("\"svg\""));
  EXPECT_TRUE (text.contains ("16_Wave"));
  EXPECT_TRUE (text.contains ("bounce"));
  EXPECT_TRUE (text.contains ("rev"));
  EXPECT_TRUE (text.contains ("hold"));

  file.deleteFile ();
}

// ── Has it drifted from the clip it came from? ───────────────────────────

namespace
{
juce::File
aClipOnDisk (juce::String const &name, ClipSettings const &settings)
{
  Clip clip;
  clip.name = name.toStdString ();
  clip.svg = "16_Wave";
  clip.settings = settings;

  auto const file = tempClip (name + ".json");
  ClipFile::save (clip, file);
  return file;
}
}

TEST (ClipFile, AFreshlyFilledSlotHasNotDrifted)
{
  Pattern pattern;
  pattern.setSpin (3);
  pattern.setReach (0.4f);

  auto const file = aClipOnDisk ("a3-drift-fresh", clipSettingsFrom (pattern));

  EXPECT_FALSE (clipHasDrifted (pattern, file));

  file.deleteFile ();
}

TEST (ClipFile, TurningSomethingIsDrift)
{
  Pattern pattern;
  auto const file = aClipOnDisk ("a3-drift-turned", clipSettingsFrom (pattern));

  pattern.setSpin (4);
  EXPECT_TRUE (clipHasDrifted (pattern, file));

  file.deleteFile ();
}

// The point of comparing rather than flagging: turning it back is not drift.
TEST (ClipFile, TurningItBackIsNotDrift)
{
  Pattern pattern;
  auto const file = aClipOnDisk ("a3-drift-back", clipSettingsFrom (pattern));

  pattern.setSpin (4);
  ASSERT_TRUE (clipHasDrifted (pattern, file));

  pattern.setSpin (0);
  EXPECT_FALSE (clipHasDrifted (pattern, file));

  file.deleteFile ();
}

// A slot with no clip behind it has nothing to have drifted from. It must not
// come up marked unsaved, or the mark would mean nothing.
TEST (ClipFile, WithNoClipThereIsNoDrift)
{
  Pattern pattern;
  pattern.setSpin (7);

  EXPECT_FALSE (clipHasDrifted (pattern, tempClip ("a3-drift-absent.json")));
}

// ── Writing settings back ────────────────────────────────────────────────

TEST (ClipFile, SavingWritesTheSettingsBack)
{
  Pattern pattern;
  auto const file = aClipOnDisk ("a3-save-back", clipSettingsFrom (pattern));

  pattern.setSpin (5);
  pattern.setEndAction (EndAction::Bounce);
  ASSERT_TRUE (saveClipSettings (pattern, file));

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_EQ (read->settings.spin, 5);
  EXPECT_EQ (read->settings.endAction, EndAction::Bounce);
  EXPECT_FALSE (clipHasDrifted (pattern, file)) << "still shows as unsaved";

  file.deleteFile ();
}

// The trap a rewrite-from-scratch would fall into: a clip's former names are
// how a session on another stick still finds it. Dropping them on every save
// would break those sessions weeks later, somewhere else, with nothing to
// point at the cause.
TEST (ClipFile, SavingKeepsWhatIsNotASetting)
{
  Clip clip;
  clip.name = "Wave slow";
  clip.aka = { "Wave 2", "Wave" };
  clip.svg = "16_Wave";

  auto const file = tempClip ("a3-save-identity.json");
  ASSERT_TRUE (ClipFile::save (clip, file));

  Pattern pattern;
  pattern.setSpin (2);
  ASSERT_TRUE (saveClipSettings (pattern, file));

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_EQ (read->name, "Wave slow");
  EXPECT_EQ (read->svg, "16_Wave");
  ASSERT_EQ (read->aka.size (), 2u) << "the former names were dropped";
  EXPECT_EQ (read->aka[0], "Wave 2");
  EXPECT_EQ (read->settings.spin, 2);

  file.deleteFile ();
}

TEST (ClipFile, SavingIntoNothingFails)
{
  Pattern pattern;
  EXPECT_FALSE (
      saveClipSettings (pattern, tempClip ("a3-save-absent.json")));
}

// ── Finding a name nobody is using ───────────────────────────────────────

TEST (ClipFile, AFreeNameIsTheOneAskedForWhenNobodyHasIt)
{
  auto const dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("a3-free-name-empty");
  dir.deleteRecursively ();
  dir.createDirectory ();

  EXPECT_EQ (freeClipName (dir, "Wave"), "Wave");

  dir.deleteRecursively ();
}

TEST (ClipFile, AFreeNameCountsUpPastWhatIsTaken)
{
  auto const dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("a3-free-name-taken");
  dir.deleteRecursively ();
  dir.createDirectory ();

  dir.getChildFile ("Wave.json").replaceWithText ("{}");
  EXPECT_EQ (freeClipName (dir, "Wave"), "Wave 2");

  dir.getChildFile ("Wave 2.json").replaceWithText ("{}");
  EXPECT_EQ (freeClipName (dir, "Wave"), "Wave 3");

  // ... and a different base is untouched by any of it.
  EXPECT_EQ (freeClipName (dir, "Circle"), "Circle");

  dir.deleteRecursively ();
}
