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

#include <ShippedSkin.hh>

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
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);

  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));
  EXPECT_TRUE (editor.canTypeBrowsedRow ());
}

TEST (SkinEditorTyping, TypingAPortOpensAFieldHoldingItsCurrentValue)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);
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
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);

  // Somebody typed the host first, and left the keyboard on screen.
  ASSERT_TRUE (browseTo (editor, "oscReceiver.host"));
  ASSERT_TRUE (editor.beginTypingBrowsedRow ());
  editor.finishNaming ();

  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));
  EXPECT_TRUE (editor.beginTypingBrowsedRow ());
  EXPECT_TRUE (editor.isNaming ());
}

// Pressing the encoder is how somebody reaches a value, and on a port that has
// to mean typing: turning is fine for a tuning number watched on the sphere,
// and useless across ten thousand port numbers. Config pages therefore type
// their numbers; the skin editor keeps turning them, which is the whole point
// of editing a skin on the device.

TEST (SkinEditorPress, PressingAPortRowOpensTypingOnAConfigPage)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);
  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));

  editor.toggleEditing ();

  EXPECT_TRUE (editor.isNaming ());
  EXPECT_FALSE (editor.isEditing ()) << "typing, not armed for turning";
  EXPECT_EQ (editor.typedText ().trim (), "7771");
}

TEST (SkinEditorPress, ATypedPortFromTheEncoderReachesTheDocument)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);
  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));

  editor.toggleEditing ();
  for (int i = 0; i < 8; ++i)
    editor.backspaceName ();
  for (auto c : { '7', '7', '9', '9' })
    editor.typeIntoName ((juce::juce_wchar)c);
  editor.finishNaming ();

  EXPECT_EQ ((int)editor.getSkin ()["oscReceiver"]["port"], 7799);
}

TEST (SkinEditorPress, PressingANumberInTheSkinEditorStillArmsItForTurning)
{
  SkinEditorComponent editor;
  editor.setDocument (juce::JSON::parse (R"({"sphereScale": 0.62})"), "default",
                      true, SkinEditorComponent::Numbers::Turned);
  ASSERT_TRUE (browseTo (editor, "sphereScale"));

  editor.toggleEditing ();

  EXPECT_TRUE (editor.isEditing ());
  EXPECT_FALSE (editor.isNaming ()) << "a skin value is dialled, not typed";

  editor.navigate (1);
  EXPECT_NE ((double)editor.getSkin ()["sphereScale"], 0.62);
}

// A host was always typed on press; that must not have changed with the
// numbers.
TEST (SkinEditorPress, PressingAHostRowStillOpensTyping)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);
  ASSERT_TRUE (browseTo (editor, "oscReceiver.host"));

  editor.toggleEditing ();

  EXPECT_TRUE (editor.isNaming ());
  EXPECT_EQ (editor.typedText ().trim (), "0.0.0.0");
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
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);
  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));
  ASSERT_TRUE (editor.beginTypingBrowsedRow ());

  for (int i = 0; i < 8; ++i)
    editor.backspaceName ();
  for (auto c : { '9', '0', '0', '1' })
    editor.typeIntoName ((juce::juce_wchar)c);
  editor.finishNaming ();

  EXPECT_EQ ((int)editor.getSkin ()["oscReceiver"]["port"], 9001);
}


// A tap names the row it wants outright, where the encoder has to turn past
// every row in between.
TEST (SkinEditorTouch, TappingARowBrowsesIt)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);

  editor.browseRow (1);
  EXPECT_EQ (editor.browsedRowIndex (), 1);

  editor.browseRow (0);
  EXPECT_EQ (editor.browsedRowIndex (), 0);
}

TEST (SkinEditorTouch, BrowsingOutsideTheListIsIgnored)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);

  editor.browseRow (0);

  editor.browseRow (-1);
  EXPECT_EQ (editor.browsedRowIndex (), 0);

  editor.browseRow (1000000);
  EXPECT_EQ (editor.browsedRowIndex (), 0);
}

// Browsing lets an armed row go, the same way turning to another row does —
// otherwise the next drag would edit a row nobody is looking at.
TEST (SkinEditorTouch, BrowsingAnotherRowDisarmsTheOldOne)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Turned);

  editor.browseRow (1);
  editor.toggleEditing ();
  ASSERT_TRUE (editor.isEditing ());

  editor.browseRow (0);
  EXPECT_FALSE (editor.isEditing ());
}

// The window of drawn rows has to keep the browsed row in it, or a tap lands
// on a row the finger cannot see.
TEST (SkinEditorTouch, TheDrawnWindowAlwaysHoldsTheBrowsedRow)
{
  SkinEditorComponent editor;
  editor.setSkin (juce::JSON::parse (R"({"a": 1, "b": 2, "c": 3, "d": 4,
                                         "e": 5, "f": 6, "g": 7, "h": 8,
                                         "i": 9, "j": 10, "k": 11, "l": 12})"),
                  "probe");
  editor.setBounds (0, 0, 768, 400);

  for (int row = 0; row < 12; ++row)
    {
      editor.browseRow (row);

      auto const first = editor.firstVisibleRow ();
      EXPECT_LE (first, row);
      EXPECT_LT (row, first + editor.visibleRowCount ());
    }
}


// Dragging a value must only ever turn a number. Arming any other kind of
// row fires it — and on a colour row that opens the picker, which is what
// dragging the skin's height scale upwards used to do: the window scrolled,
// the hit area came to stand for the colour row above, and the drag armed it.
TEST (SkinEditorTouch, OnlyATurnableNumberCanBeDragged)
{
  SkinEditorComponent editor;
  editor.setSkin (shippedSkin (), "default");

  bool sawColourPicker = false;
  editor.onColourPicked = [&] (juce::String const &) { sawColourPicker = true; };

  // Every row in the shipped skin, including its colours and its three
  // action rows: none of them may be turnable except plain numbers.
  for (int row = 0; row < 64; ++row)
    {
      editor.browseRow (row);
      if (editor.browsedRowIndex () != row)
        break; // ran off the end of the list

      if (!editor.canTurnBrowsedRow ())
        continue;

      // A row that says it can be turned must survive being turned without
      // asking anyone for a colour.
      editor.toggleEditing ();
      editor.navigate (1);
      EXPECT_FALSE (sawColourPicker) << "row " << row;
      if (editor.isEditing ())
        editor.toggleEditing ();
    }
}

TEST (SkinEditorTouch, ActionRowsAreNotTurnable)
{
  SkinEditorComponent editor;
  editor.setSkin (shippedSkin (), "default");

  // Save, Save As New, Rename — the rows above the parameters.
  for (int row = 0; row < 3; ++row)
    {
      editor.browseRow (row);
      EXPECT_FALSE (editor.canTurnBrowsedRow ()) << "action row " << row;
    }
}

// A config page types its numbers rather than turning them, so a drag must
// not arm one there either.
TEST (SkinEditorTouch, TypedNumbersAreNotTurnable)
{
  SkinEditorComponent editor;
  editor.setDocument (networkSlice (), "Network", false,
                      SkinEditorComponent::Numbers::Typed);

  ASSERT_TRUE (browseTo (editor, "oscReceiver.port"));
  EXPECT_FALSE (editor.canTurnBrowsedRow ());
}
