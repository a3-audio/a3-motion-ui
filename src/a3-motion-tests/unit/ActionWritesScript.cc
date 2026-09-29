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

#include <a3-motion-ui/components/ActionComponent.hh>
#include <a3-motion-ui/components/ActionKnobs.hh>
#include <a3-motion-ui/components/PendingScriptWrites.hh>
#include <a3-motion-ui/components/ScriptPanel.hh>

using namespace a3;

TEST (ActionWritesScript, EveryAudioControlHasItsScriptLine)
{
  EXPECT_STREQ (actionControlScriptName (ActionComponent::Attack), "attack");
  EXPECT_STREQ (actionControlScriptName (ActionComponent::EnvelopeMax), "envelopeMax");
  EXPECT_STREQ (actionControlScriptName (ActionComponent::FreqDecay), "freqDecay");
  EXPECT_STREQ (actionControlScriptName (ActionComponent::QMax), "qMax");
  EXPECT_STREQ (actionControlScriptName (ActionComponent::ActMode), "act");
  EXPECT_STREQ (actionControlScriptName (-1), "");
}

TEST (PendingScriptWrites, ManyStepsOnOneFileAreOneWrite)
{
  PendingScriptWrites pending;
  juce::File const file ("/tmp/a3-test-one.scd");
  pending.put (file, "a");
  pending.put (file, "b");
  pending.put (file, "c");
  auto const writes = pending.take ();
  ASSERT_EQ (writes.size (), 1u);
  EXPECT_EQ (writes[0].second, "c");
  EXPECT_TRUE (pending.empty ());
}

TEST (PendingScriptWrites, TwoFilesAreTwoWrites)
{
  PendingScriptWrites pending;
  pending.put (juce::File ("/tmp/a3-test-a.scd"), "a");
  pending.put (juce::File ("/tmp/a3-test-b.scd"), "b");
  EXPECT_EQ (pending.take ().size (), 2u);
}

TEST (PendingScriptWrites, AFailedWriteIsReported)
{
  auto const dir = juce::File::createTempFile ("a3dir");
  dir.createDirectory ();
  auto const good = dir.getChildFile ("Good.scd");
  auto const bad = dir.getChildFile ("missing-folder").getChildFile ("Bad.scd");
  auto const failed = writeAll ({ { good, "~spin = 1;\n" }, { bad, "x" } });
  EXPECT_EQ (good.loadFileAsString (), "~spin = 1;\n");
  ASSERT_EQ (failed.size (), 1);
  EXPECT_EQ (failed[0], "Bad");
  dir.deleteRecursively ();
}

TEST (PendingScriptWrites, EveryButtonHoldingTheFileIsFound)
{
  juce::File const shared ("/tmp/Lift Up.scd");
  juce::File const other ("/tmp/Spin.scd");
  std::vector<std::array<juce::File, numActionButtons>> files (2);
  files[0][1] = shared;
  files[0][4] = other;
  files[1][1] = shared;
  auto const holding = buttonsHoldingFile (files, shared);
  ASSERT_EQ (holding.size (), 2u);
  EXPECT_EQ (holding[0], std::make_pair (0, 1));
  EXPECT_EQ (holding[1], std::make_pair (1, 1));
}

TEST (ActionWritesScript, AnEditReachesAnUnsavedEditorWithoutLosingTheTyping)
{
  ScriptPanel panel;
  panel.setScript ("~spin = 1;\n");
  panel.markSaved ();
  panel.offerScript ("~spin = 1;\n// typed\n"); // unsaved text, as if typed
  ASSERT_TRUE (panel.hasUnsavedChanges ());
  panel.applyEdit ("~spin = 4;\n// typed\n");
  EXPECT_EQ (panel.script (), "~spin = 4;\n// typed\n");
  EXPECT_TRUE (panel.hasUnsavedChanges ()) << "still unsaved";
}

TEST (ActionWritesScript, AnEditReachesAnEditorBeingTypedInto)
{
  ScriptPanel panel;
  panel.setScript ("~spin = 1;\n");
  panel.markSaved ();
  panel.applyEdit ("~spin = 4;\n");
  EXPECT_EQ (panel.script (), "~spin = 4;\n");
}
