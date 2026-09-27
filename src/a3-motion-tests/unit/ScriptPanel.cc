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
