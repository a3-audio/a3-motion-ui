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
  for (auto *child : panel.getChildren ())
    if (dynamic_cast<juce::CodeEditorComponent *> (child) != nullptr)
      {
        found = true;
        EXPECT_FALSE (child->isOpaque ());
      }
  EXPECT_TRUE (found);
}
