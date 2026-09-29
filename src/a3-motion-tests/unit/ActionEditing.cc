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

#include <a3-motion-ui/components/ActionEditing.hh>

using namespace a3;

namespace
{
juce::File const bloom ("/tmp/actions/Bloom.scd");
juce::File const ground ("/tmp/actions/Ground.scd");
}

// Save re-runs a script on every clip that fires it, not only the shown one:
// otherwise an edit is heard only after assigning it again.
TEST (ActionEditing, EveryClipFiringTheFileIsFound)
{
  std::vector<std::vector<juce::File>> const slots{
    { bloom, {} }, { ground, bloom }, { {}, {} }, { bloom, ground }
  };
  auto const firing = slotsFiring (bloom, slots);
  std::vector<SlotRef> const expected{ { 0, 0 }, { 1, 1 }, { 3, 0 } };
  EXPECT_EQ (firing, expected);
}

TEST (ActionEditing, NoFileIsFiredByNobody)
{
  std::vector<std::vector<juce::File>> const slots{ { {}, {} } };
  EXPECT_TRUE (slotsFiring (juce::File{}, slots).empty ());
}

// Unsaved changes hold the list: a tap on another row, Rename and Delete
// wait for Save or Cancel (maintainer, 2026-09-27).
TEST (ActionEditing, UnsavedScriptHoldsTheList)
{
  EXPECT_TRUE (listWaitsFor (true));
  EXPECT_FALSE (listWaitsFor (false));
}

// Save as re-points only the clip EDIT came from, and only once: the origin
// is taken by the first Save as, so a second one on another script re-points
// nobody (final review, 2026-09-27). Opened from the tab, nobody at all.
TEST (ActionEditing, TheEditOriginIsRepointedOnce)
{
  std::optional<SlotRef> none;
  EXPECT_FALSE (takeEditOrigin (none).has_value ());

  std::optional<SlotRef> origin = SlotRef{ 2, 1 };
  auto const first = takeEditOrigin (origin);
  ASSERT_TRUE (first.has_value ());
  EXPECT_EQ (*first, (SlotRef{ 2, 1 }));
  EXPECT_FALSE (takeEditOrigin (origin).has_value ()) << "a second Save as";
}

// The panel holds the file it was loaded from. When the list's chosen row
// moves under it -- a clip change, a delete, FILES reopened -- it reloads,
// unless it has unsaved text: then the row goes back to the panel's file
// rather than the text being written into a different one (final review).
TEST (ActionEditing, ThePanelFollowsTheRowUnlessItHoldsUnsavedText)
{
  EXPECT_EQ (panelSyncFor (bloom, bloom, false), PanelSync::Keep);
  EXPECT_EQ (panelSyncFor (bloom, bloom, true), PanelSync::Keep)
      << "unsaved text on the same file stays";
  EXPECT_EQ (panelSyncFor (ground, bloom, false), PanelSync::Reload);
  EXPECT_EQ (panelSyncFor (ground, bloom, true), PanelSync::HoldRow);
}

// What a script gets wrong, for the panel's error strip.
TEST (ActionEditing, AScriptSaysWhatIsWrongWithIt)
{
  EXPECT_TRUE (scriptErrorsOf ("~base = 0.5;").isEmpty ());
  EXPECT_FALSE (scriptErrorsOf ("~base = ;").isEmpty ());
}

// A copy is named after what it came from ("Bloom 2"), a new one "Action".
TEST (ActionEditing, ACopyIsNamedAfterItsOrigin)
{
  EXPECT_EQ (copyBaseFor (bloom), "Bloom");
  EXPECT_EQ (copyBaseFor (juce::File{}), "Action");
}

// Every file in FILES is edited as text (2026-09-27): what is wrong with it
// is said in the panel, whatever kind it is.
TEST (ActionEditing, EveryKindOfFileSaysWhatIsWrongWithIt)
{
  juce::File const set ("/p/sessions/user/Set.json");
  juce::File const clip ("/p/clips/user/Arc.svg");

  EXPECT_TRUE (fileErrorsOf ("{ \"name\": \"x\" }", set).isEmpty ());
  EXPECT_FALSE (fileErrorsOf ("{ \"name\": ", set).isEmpty ());
  EXPECT_TRUE (fileErrorsOf ("<svg viewBox=\"-1 -1 2 2\"/>", clip).isEmpty ());
  EXPECT_FALSE (fileErrorsOf ("<svg viewBox=", clip).isEmpty ());
  EXPECT_FALSE (fileErrorsOf ("<svg><g></svg>", clip).isEmpty ())
      << "tags that do not match";
  EXPECT_FALSE (fileErrorsOf ("~base = ;", bloom).isEmpty ());
}

// A broken set or SVG is not written: it would make the file unloadable. A
// script with an error still is -- the lines that read still run, as before.
TEST (ActionEditing, ABrokenSetOrSvgIsNotWritten)
{
  juce::StringArray const wrong{ "line 1: something" };
  EXPECT_TRUE (errorsBlockSaving (wrong, juce::File ("/p/sessions/user/S.json")));
  EXPECT_TRUE (errorsBlockSaving (wrong, juce::File ("/p/system/Arc.svg")));
  EXPECT_FALSE (errorsBlockSaving (wrong, bloom));
  EXPECT_FALSE (errorsBlockSaving ({}, juce::File ("/p/sessions/user/S.json")));
}

// -- Cue (library v2, 2026-09-28) ----------------------------------------------
// A Cue button carries the clip its script names, resolved when the script is
// put on the button -- never on the press.

namespace
{
juce::File
aClipsDirHolding (juce::StringArray const &names)
{
  auto const dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("a3-cue-clips");
  dir.deleteRecursively ();
  dir.getChildFile ("system").createDirectory ();
  for (auto const &n : names)
    dir.getChildFile ("system").getChildFile (n + ".json").replaceWithText ("{}");
  return dir;
}
}

TEST (CueClip, NoClipLineIsNoCue)
{
  auto const t = cueClipFor (std::nullopt, aClipsDirHolding ({}));
  EXPECT_FALSE (t.file.exists ());
  EXPECT_TRUE (t.error.isEmpty ());
}

TEST (CueClip, AShippedClipIsFoundByName)
{
  auto const dir = aClipsDirHolding ({ "Peak Anthem" });
  auto const t = cueClipFor (juce::String ("Peak Anthem"), dir);
  EXPECT_EQ (t.file, dir.getChildFile ("system/Peak Anthem.json"));
  EXPECT_TRUE (t.error.isEmpty ());
}

// Review focus 2: a clip renamed or deleted later leaves the button saying so
// and doing nothing -- never a silent Default.
TEST (CueClip, AMissingClipIsAnErrorAndNoCue)
{
  auto const t = cueClipFor (juce::String ("Peak Gone"), aClipsDirHolding ({}));
  EXPECT_FALSE (t.file.exists ());
  EXPECT_TRUE (t.error.contains ("Peak Gone")) << t.error;
}

// Library v2 review: what a press on a Cue button does. The Cue is decided
// before anything else, so a Cue whose clip is gone does nothing -- it never
// falls through to an ordinary accent (review #1) -- and a take waiting for
// SAVE is never thrown away by one tap on stage (review #4).
TEST (CuePress, AButtonThatIsNoCueGoesOnAsAnAction)
{
  EXPECT_EQ (cuePressFor (false, false, false, false), CuePress::NotACue);
}

TEST (CuePress, ACueWithItsClipLoadsIt)
{
  EXPECT_EQ (cuePressFor (true, true, false, false), CuePress::Load);
}

TEST (CuePress, ACueWhoseClipIsGoneDoesNothing)
{
  EXPECT_EQ (cuePressFor (true, false, false, false), CuePress::NoClip);
}

TEST (CuePress, ACueWaitsForATakeToBeSaved)
{
  EXPECT_EQ (cuePressFor (true, true, false, true), CuePress::TakeWaiting);
}

TEST (CuePress, ACueNeverGoesOverATakeGoingIn)
{
  EXPECT_EQ (cuePressFor (true, true, true, false), CuePress::Recording);
  EXPECT_EQ (cuePressFor (true, true, true, true), CuePress::Recording);
}

// What a Cue puts on the channel. The ACTION page shows a Cue button's motion
// and writes a turned value into its script (~reach = 0.82;), so the press has
// to carry those lines onto the clip it cues -- it used to load the clip file
// and drop every other line, which on a channel already playing that clip
// left the press doing nothing at all.
namespace
{
ClipSettings
aCuedClip ()
{
  ClipSettings clip;
  clip.reach = 0.35f;
  clip.spin = 1;
  clip.reachLfo = 2;
  clip.envelopeAttack = 2;
  clip.envelopeMax = 1.f;
  clip.actMode = ActMode::OneShot;
  return clip;
}
}

TEST (CueClip, ACuePutsItsOwnLinesOnTheClip)
{
  auto const cued = cuedClipSettings (
      "~clip = \"Warmup Halo\";\n~reach = 0.82;\n~spin = -2;\n", aCuedClip (),
      1);

  EXPECT_FLOAT_EQ (cued.reach, 0.82f);
  EXPECT_EQ (cued.spin, -2);
  EXPECT_EQ (cued.reachLfo, 2) << "a line the script leaves out stays the clip's";
}

// A Cue has no accent: its ceilings and envelope lines must not change how the
// clip's own ACT is played afterwards.
TEST (CueClip, ACueLeavesTheClipsFeelAlone)
{
  auto const cued = cuedClipSettings (
      "~clip = \"Warmup Halo\";\n~attack = 0;\n~envelopeMax = 0;\n"
      "~act = \\hold;\n",
      aCuedClip (), 1);

  EXPECT_EQ (actionFeelFrom (cued), actionFeelFrom (aCuedClip ()));
}

TEST (CueClip, ACueWithOnlyItsClipLinePutsTheClipAsItIs)
{
  EXPECT_EQ (cuedClipSettings ("~clip = \"Warmup Halo\";\n", aCuedClip (), 1),
             aCuedClip ());
}
