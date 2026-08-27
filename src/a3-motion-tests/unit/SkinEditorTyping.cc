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

#include <a3-motion-ui/components/SkinEditorComponent.hh>

using namespace a3;

namespace
{
// The Network page as A3MotionUIComponent::openConfigPage builds it: a slice
// of config.json with no skin actions on top.
juce::var
networkSlice ()
{
  return juce::JSON::parse (
      R"({"oscReceiver": {"host": "0.0.0.0", "port": 7771}})");
}

/** Browse to the row with this path, the way the encoder would. */
bool
browseTo (SkinEditorComponent &editor, juce::String const &path)
{
  for (int i = 0; i < 64; ++i)
    {
      if (editor.browsedPath () == path)
        return true;
      editor.navigate (1);
    }
  return false;
}
}

TEST (SkinEditorTyping, APortIsARowThatCanBeTyped)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false);

  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));
  EXPECT_TRUE (editor.canTypeBrowsedRow ());
}

TEST (SkinEditorTyping, TypingAPortOpensAFieldHoldingItsCurrentValue)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false);
  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));

  EXPECT_TRUE (editor.beginTypingBrowsedRow ());
  EXPECT_TRUE (editor.isNaming ());
  EXPECT_EQ (editor.typedText ().trim (), "7771");
}

// The reported failure: the keyboard icon only began typing when it was the
// press that put the keyboard on screen. With the keyboard already up — which
// is the normal state once somebody has been typing — the same press onto a
// port row did nothing at all, and the port stayed encoder-only.
TEST (SkinEditorTyping, APortCanBeTypedWhileTheKeyboardIsAlreadyUp)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false);

  // Somebody typed the host first, and left the keyboard on screen.
  ASSERT_TRUE (browseTo (editor, "oscReceiver.host"));
  ASSERT_TRUE (editor.beginTypingBrowsedRow ());
  editor.finishNaming ();

  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));
  EXPECT_TRUE (editor.beginTypingBrowsedRow ());
  EXPECT_TRUE (editor.isNaming ());
}

// The typing field used to be two list rows tall whatever the font, while the
// text inside it was drawn centred and the caret was nailed to the field's
// bottom edge. The taller the field, the further the caret drifted from the
// text it belonged to.

TEST (SkinEditorTypingField, TheFieldFollowsTheTextRatherThanTheRowCount)
{
  auto const rowHeight = (int)std::lround (15.f * 1.9f); // body 15
  auto const height = typingFieldHeight (18.f, rowHeight); // header 18

  EXPECT_LT (height, 2 * rowHeight);
  EXPECT_GE (height, rowHeight);
}

TEST (SkinEditorTypingField, TheFieldGrowsWithTheFont)
{
  EXPECT_GT (typingFieldHeight (30.f, 20), typingFieldHeight (15.f, 20));
}

TEST (SkinEditorTypingField, TheCaretSitsJustUnderTheText)
{
  for (auto size : { 12.f, 18.f, 30.f, 44.f })
    {
      auto const font = juce::Font (juce::FontOptions (size));
      auto const rowHeight = (int)std::lround (size * 1.9f);
      juce::Rectangle<int> field{ 0, 0, 400,
                                  typingFieldHeight (size, rowHeight) };

      auto const caret = typingCaretY (field, font);
      auto const textBottom
          = (float)field.getCentreY () + font.getHeight () * 0.5f;

      EXPECT_GE (caret, textBottom - 1.f) << "size " << size;
      EXPECT_LE (caret - textBottom, 3.f) << "size " << size;
      EXPECT_LT (caret, (float)field.getBottom ()) << "size " << size;
    }
}

TEST (SkinEditorTyping, ATypedPortReachesTheDocument)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false);
  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));
  ASSERT_TRUE (editor.beginTypingBrowsedRow ());

  for (int i = 0; i < 8; ++i)
    editor.backspaceName ();
  for (auto c : { '9', '0', '0', '1' })
    editor.typeIntoName ((juce::juce_wchar)c);
  editor.finishNaming ();

  EXPECT_EQ ((int)editor.getSkin ()["oscReceiver"]["port"], 9001);
}
