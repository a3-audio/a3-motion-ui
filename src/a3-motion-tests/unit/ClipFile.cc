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
  clip.settings.fadeReach = 0.8f;
  clip.settings.bridgeBias = 2;

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
  EXPECT_FLOAT_EQ (read->settings.fadeReach, 0.8f);
  EXPECT_EQ (read->settings.bridgeBias, 2);

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
  EXPECT_FLOAT_EQ (read->settings.fadeReach, defaults.fadeReach);
  EXPECT_FLOAT_EQ (read->settings.reach, defaults.reach);

  file.deleteFile ();
}

// Rubbish must not come back as a clip full of noise in the middle of a set:
// it fails to load, and the slot stays empty and says so.
// The cutoff and the resonance shared one envelope for a day, written as
// "filter*". Clips saved in that day carry those names and no others, and
// what they meant was the cutoff -- the resonance had no envelope of its own
// to lose. A file that quietly loaded them as defaults would be a clip whose
// sweep went missing without saying so.
TEST (ClipFile, AClipFromBeforeTheSplitKeepsItsCutoffEnvelope)
{
  auto const file = tempClip ("a3-clip-legacy-filter.json");
  file.replaceWithText (R"({ "name": "Old", "svg": "16_Circle",
                             "filterAttack": 5, "filterDecay": 1,
                             "filterMax": 0.75 })");

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());

  EXPECT_EQ (read->settings.freqAttack, 5);
  EXPECT_EQ (read->settings.freqDecay, 1);
  EXPECT_FLOAT_EQ (read->settings.freqMax, 0.75f);

  ClipSettings const defaults;
  EXPECT_EQ (read->settings.qAttack, defaults.qAttack);
  EXPECT_FLOAT_EQ (read->settings.qMax, defaults.qMax)
      << "the resonance had no envelope back then and must not inherit one";

  file.deleteFile ();
}

// And the new names win where a file carries both, which is what a file
// written by this version and then edited by hand would look like.
TEST (ClipFile, TheNewNamesWinOverTheOldOnes)
{
  auto const file = tempClip ("a3-clip-both-names.json");
  file.replaceWithText (R"({ "name": "Both", "svg": "16_Circle",
                             "filterAttack": 5, "freqAttack": 2 })");

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_EQ (read->settings.freqAttack, 2);

  file.deleteFile ();
}

// A clip written before the elevation base carries mirrorSouth instead, and
// what that meant was "the middle of the trajectory sits at the south pole".
// That is a base of 1, so it loads as one and the take sounds as it did.
TEST (ClipFile, AClipFromBeforeTheBaseKeepsItsPole)
{
  auto const south = tempClip ("a3-clip-mirror-south.json");
  south.replaceWithText (R"({ "name": "Old", "svg": "16_Circle",
                              "mirrorSouth": true })");

  auto const read = ClipFile::load (south);
  ASSERT_TRUE (read.has_value ());
  EXPECT_FLOAT_EQ (read->settings.elevationBase, 1.f);

  // A northern one is a base of zero, which is also the default -- so a clip
  // that never said anything about it is unaffected.
  auto const north = tempClip ("a3-clip-mirror-north.json");
  north.replaceWithText (R"({ "name": "Old", "svg": "16_Circle" })");

  auto const plain = ClipFile::load (north);
  ASSERT_TRUE (plain.has_value ());
  EXPECT_FLOAT_EQ (plain->settings.elevationBase, 0.f);

  south.deleteFile ();
  north.deleteFile ();
}

// A file that names the base itself wins over the old flag -- that is what a
// clip written by this version looks like once it has been saved again.
TEST (ClipFile, TheBaseWinsOverTheOldFlag)
{
  auto const file = tempClip ("a3-clip-base-and-flag.json");
  file.replaceWithText (R"({ "name": "Both", "svg": "16_Circle",
                             "mirrorSouth": true, "elevationBase": 0.25 })");

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_FLOAT_EQ (read->settings.elevationBase, 0.25f);

  file.deleteFile ();
}

TEST (ClipFile, RubbishDoesNotLoad)
{
  auto const file = tempClip ("a3-clip-rubbish.json");
  file.replaceWithText ("this is not json at all {{{");

  EXPECT_FALSE (ClipFile::load (file).has_value ());

  file.deleteFile ();
}

// A clip may name no shape: that is a settings preset. Applying it leaves the
// slot's shape alone and changes only how it is played -- which is what makes
// a browser of settings possible instead of a browser of seventy shapes that
// all carry the same defaults.
TEST (ClipFile, AClipWithoutAShapeIsASettingsPreset)
{
  auto const file = tempClip ("a3-clip-shapeless.json");
  file.replaceWithText (R"({ "name": "Fast Wide", "speed": -1, "reach": 0.9 })");

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_TRUE (read->svg.empty ());
  EXPECT_EQ (read->settings.speedLog2, -1);
  EXPECT_FLOAT_EQ (read->settings.reach, 0.9f);

  file.deleteFile ();
}

// ... and a preset writes no empty "svg", so the file says what it is.
TEST (ClipFile, APresetWritesNoShape)
{
  Clip clip;
  clip.name = "Preset";
  clip.settings.spin = 2;

  auto const file = tempClip ("a3-clip-preset.json");
  ASSERT_TRUE (ClipFile::save (clip, file));

  EXPECT_FALSE (file.loadFileAsString ().contains ("\"svg\""));

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

// The fade changed its unit -- from sixteenths of a beat to a distance -- so
// old values cannot be carried over. A file still holding one is read without
// complaint and gets the new defaults; there is nothing to translate, because
// the seam machinery those values belonged to is gone.
TEST (ClipFileFade, TheNewFieldsSurviveARoundTrip)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-clipfile-fade/Bridged.json");
  file.getParentDirectory ().deleteRecursively ();

  Clip written;
  written.name = "Bridged";
  written.settings.fadeReach = 0.75f;
  written.settings.bridgeBias = -3;
  ASSERT_TRUE (ClipFile::save (written, file));

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());
  EXPECT_FLOAT_EQ (read->settings.fadeReach, 0.75f);
  EXPECT_EQ (read->settings.bridgeBias, -3);
}

TEST (ClipFileFade, AnOldFadeValueIsIgnoredRatherThanTranslated)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-clipfile-oldfade/Legacy.json");
  file.getParentDirectory ().deleteRecursively ();
  file.getParentDirectory ().createDirectory ();
  file.replaceWithText (R"({"name":"Legacy","fade":12})");

  auto const read = ClipFile::load (file);
  ASSERT_TRUE (read.has_value ());

  ClipSettings const defaults;
  EXPECT_FLOAT_EQ (read->settings.fadeReach, defaults.fadeReach);
  EXPECT_EQ (read->settings.bridgeBias, defaults.bridgeBias);
}
