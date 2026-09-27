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

#include <a3-motion-ui/components/ScriptPanel.hh>

using namespace a3;

// Text loaded from a file is what is saved: nothing to keep, nothing to lose.
TEST (ScriptPanel, ALoadedScriptIsSaved)
{
  ScriptPanel panel;
  panel.setScript ("~base = 1;");
  EXPECT_EQ (panel.script (), "~base = 1;");
  EXPECT_FALSE (panel.hasUnsavedChanges ());
}

// FROM CLIP puts text in that has not been saved anywhere yet.
TEST (ScriptPanel, OfferedTextIsUnsavedUntilKept)
{
  ScriptPanel panel;
  panel.setScript ("~base = 1;");
  panel.offerScript ("~base = 0.5;");
  EXPECT_EQ (panel.script (), "~base = 0.5;");
  EXPECT_TRUE (panel.hasUnsavedChanges ());
  panel.markSaved ();
  EXPECT_FALSE (panel.hasUnsavedChanges ());
}

// A panel nobody has typed into has nothing unsaved: a fresh document counts
// as changed until it has a save point, and that held the FILES list against
// a script that did not exist (final review fix, 2026-09-27).
TEST (ScriptPanel, AFreshPanelHasNothingUnsaved)
{
  ScriptPanel panel;
  EXPECT_FALSE (panel.hasUnsavedChanges ());
}

// JUCE's code editor calls itself opaque, but it is drawn transparent over
// the panel's darker field. Believed, a scroll repainted only the editor and
// not the ground behind it -- and over the sphere, what is behind it is the
// OpenGL picture: the trajectory showed through the script (2026-09-27).
TEST (ScriptPanel, TheEditorDoesNotClaimToBeOpaque)
{
  ScriptPanel panel;
  auto found = false;
  for (auto *frame : panel.getChildren ())
    for (auto *child : frame->getChildren ())
      if (dynamic_cast<juce::CodeEditorComponent *> (child) != nullptr)
        {
          found = true;
          EXPECT_FALSE (child->isOpaque ());
        }
  EXPECT_TRUE (found);
}

// JUCE's editor takes its tokeniser once, so a change of language builds a
// new editor -- which must keep the text, its saved state, and the fix above.
TEST (ScriptPanel, AChangeOfLanguageKeepsTheTextAndTheGround)
{
  ScriptPanel panel;
  panel.setScript ("<svg/>");
  panel.setLanguage (ScriptLanguage::Xml);
  EXPECT_EQ (panel.script (), "<svg/>");
  EXPECT_FALSE (panel.hasUnsavedChanges ());

  auto editors = 0;
  for (auto *frame : panel.getChildren ())
    for (auto *child : frame->getChildren ())
      if (dynamic_cast<juce::CodeEditorComponent *> (child) != nullptr)
        {
          ++editors;
          EXPECT_FALSE (child->isOpaque ());
        }
  EXPECT_EQ (editors, 1) << "the old editor is gone";
}

// The whole of a line should fit (maintainer, 2026-09-27: "sodass der
// editor ganz draufpasst"). Where the column is narrower than a line at the
// usual size, the font steps down until it fits; where there is room it
// stays as it is.
TEST (ScriptPanel, AColumnTooNarrowForALineTakesASmallerFont)
{
  constexpr int line = 86;
  ScriptPanel panel;
  auto const natural = panel.widthFor (line);

  panel.setColumnsToFit (line);
  panel.setBounds (0, 0, natural * 9 / 10, 400);
  EXPECT_LE (panel.widthFor (line), panel.getWidth ());

  panel.setBounds (0, 0, natural * 2, 400);
  EXPECT_EQ (panel.widthFor (line), natural) << "room enough: the usual size";
}

// JUCE's line numbers take a fixed 35 px and are set to its right edge; the
// rest was empty room left of them (maintainer, 2026-09-27). The editor sits
// shifted left in a frame that cuts that room off.
TEST (ScriptPanel, TheLineNumbersKeepOnlyTheRoomTheyNeed)
{
  ScriptPanel panel;
  panel.setBounds (0, 0, 600, 400);
  juce::CodeEditorComponent *editor = nullptr;
  for (auto *child : panel.getChildren ())
    for (auto *inner : child->getChildren ())
      if (auto *e = dynamic_cast<juce::CodeEditorComponent *> (inner))
        editor = e;
  ASSERT_NE (editor, nullptr) << "the editor stands in a frame";
  EXPECT_LT (editor->getX (), 0) << "shifted left, the empty room cut off";
}
