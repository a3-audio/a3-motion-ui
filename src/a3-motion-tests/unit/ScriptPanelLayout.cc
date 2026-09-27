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

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/ScriptPanelLayout.hh>

using namespace a3;

namespace
{
juce::Rectangle<int> const column{ 256, 60, 512, 560 };
constexpr int buttonHeight = 44;
constexpr int lineHeight = 16;
}

// Four keys in one row at the foot, a fingertip each, in the order
// FROM CLIP, Cancel, Save, Save as, none of them over the text.
TEST (ScriptPanelLayout, FourKeysAtTheFootAFingertipEach)
{
  auto const l = layOutScriptPanel (column, buttonHeight, 0, lineHeight);
  for (auto const &key : { l.fromClipButton, l.cancelButton, l.saveButton,
                           l.saveAsButton })
    {
      EXPECT_GE (key.getWidth (), fingertipSize);
      EXPECT_GE (key.getHeight (), fingertipSize);
      EXPECT_TRUE (column.contains (key));
      EXPECT_FALSE (key.intersects (l.textArea));
      EXPECT_EQ (key.getBottom (), column.getBottom ());
    }
  EXPECT_LT (l.fromClipButton.getRight (), l.cancelButton.getX () + 1);
  EXPECT_LT (l.cancelButton.getRight (), l.saveButton.getX () + 1);
  EXPECT_LT (l.saveButton.getRight (), l.saveAsButton.getX () + 1);
}

TEST (ScriptPanelLayout, NoErrorsNoStrip)
{
  auto const l = layOutScriptPanel (column, buttonHeight, 0, lineHeight);
  EXPECT_TRUE (l.errorArea.isEmpty ());
}

// Errors take their room off the text rather than covering it, and never
// more than half of it.
TEST (ScriptPanelLayout, ErrorsTakeTheirRoomOffTheText)
{
  auto const l = layOutScriptPanel (column, buttonHeight, 3, lineHeight);
  EXPECT_EQ (l.errorArea.getHeight (), 3 * lineHeight);
  EXPECT_FALSE (l.errorArea.intersects (l.textArea));
  EXPECT_LE (l.errorArea.getBottom (), l.saveButton.getY ());

  auto const many = layOutScriptPanel (column, buttonHeight, 100, lineHeight);
  EXPECT_GE (many.textArea.getHeight (), many.errorArea.getHeight ());
}

// A key is lit only when it would do something (the rule the ACTION page
// painted by). A locked shipped script offers only a copy.
TEST (ScriptPanelLayout, UnchangedScriptOffersNothingToKeep)
{
  auto const k = scriptKeysFor (false, false, true, true);
  EXPECT_FALSE (k.save);
  EXPECT_FALSE (k.saveAs);
  EXPECT_FALSE (k.cancel);
  EXPECT_TRUE (k.fromClip);
}

TEST (ScriptPanelLayout, LockedScriptOffersOnlyACopy)
{
  auto const k = scriptKeysFor (true, true, true, true);
  EXPECT_FALSE (k.save);
  EXPECT_TRUE (k.saveAs);
  EXPECT_TRUE (k.cancel);
}

TEST (ScriptPanelLayout, AChangedScriptOfYourOwnCanBeSaved)
{
  auto const k = scriptKeysFor (true, false, true, true);
  EXPECT_TRUE (k.save);
  EXPECT_TRUE (k.saveAs);
}

// FROM CLIP needs a clip on the shown slot; Save needs a file to write to.
TEST (ScriptPanelLayout, NothingOnTheSlotNothingToTakeFrom)
{
  EXPECT_FALSE (scriptKeysFor (false, false, true, false).fromClip);
  EXPECT_FALSE (scriptKeysFor (true, false, false, true).save);
  EXPECT_TRUE (scriptKeysFor (true, false, false, true).saveAs);
}
