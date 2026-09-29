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

#include <a3-motion-ui/components/BarKeyboardModel.hh>

using namespace a3;

namespace
{

KeyDef
letter (char c)
{
  return { KeyAction::Character, static_cast<juce::juce_wchar> (c), 1 };
}

KeyDef
action (KeyAction a)
{
  return { a, 0, 1 };
}

/** What a sequence of presses types, read off the key events. */
juce::String
typed (KeyboardState &state, std::vector<KeyDef> const &keys,
       bool shiftHeld = false)
{
  juce::String text;
  for (auto const &key : keys)
    {
      auto const outcome = pressKey (state, key, shiftHeld);
      if (outcome.key.has_value ())
        text += juce::String::charToString (outcome.key->getTextCharacter ());
    }
  return text;
}

}

TEST (BarKeyboardModel, ALetterTypesItselfInLowerCase)
{
  KeyboardState state;
  EXPECT_EQ (typed (state, { letter ('a'), letter ('b') }), "ab");
}

TEST (BarKeyboardModel, ShiftOnceCapitalisesOnlyTheNextLetter)
{
  KeyboardState state;
  EXPECT_EQ (typed (state, { action (KeyAction::Shift), letter ('b'),
                             letter ('b') }),
             "Bb");
  EXPECT_EQ (state.shift, ShiftState::Off);
}

TEST (BarKeyboardModel, ShiftTwiceLocksAndAThirdTimeReleases)
{
  KeyboardState state;
  pressKey (state, action (KeyAction::Shift), false);
  pressKey (state, action (KeyAction::Shift), false);
  EXPECT_EQ (state.shift, ShiftState::Locked);
  EXPECT_EQ (typed (state, { letter ('a'), letter ('b') }), "AB");
  pressKey (state, action (KeyAction::Shift), false);
  EXPECT_EQ (state.shift, ShiftState::Off);
  EXPECT_EQ (typed (state, { letter ('a') }), "a");
}

TEST (BarKeyboardModel, TheHeldPanelShiftCapitalisesToo)
{
  KeyboardState state;
  EXPECT_EQ (typed (state, { letter ('a') }, true), "A");
  // ...without latching anything.
  EXPECT_EQ (state.shift, ShiftState::Off);
}

TEST (BarKeyboardModel, UmlautsCapitaliseAndSharpSStays)
{
  KeyboardState state;
  state.shift = ShiftState::Locked;
  auto const u = juce::juce_wchar (0x00FC);
  auto const sharpS = juce::juce_wchar (0x00DF);
  auto const out = pressKey (state, { KeyAction::Character, u, 1 }, false);
  ASSERT_TRUE (out.key.has_value ());
  EXPECT_EQ (out.key->getTextCharacter (), juce::juce_wchar (0x00DC));
  auto const s = pressKey (state, { KeyAction::Character, sharpS, 1 }, false);
  ASSERT_TRUE (s.key.has_value ());
  EXPECT_EQ (s.key->getTextCharacter (), sharpS);
}

TEST (BarKeyboardModel, EditingKeysBecomeTheKeyboardsOwnKeys)
{
  KeyboardState state;
  auto expectKey = [&] (KeyAction a, int keyCode) {
    auto const out = pressKey (state, action (a), false);
    ASSERT_TRUE (out.key.has_value ());
    EXPECT_TRUE (out.key->isKeyCode (keyCode));
    EXPECT_FALSE (out.hide);
  };
  expectKey (KeyAction::Backspace, juce::KeyPress::backspaceKey);
  expectKey (KeyAction::Enter, juce::KeyPress::returnKey);
  expectKey (KeyAction::Left, juce::KeyPress::leftKey);
  expectKey (KeyAction::Right, juce::KeyPress::rightKey);
  expectKey (KeyAction::Escape, juce::KeyPress::escapeKey);
}

TEST (BarKeyboardModel, SpaceIsTheSpaceKeyAndTypesASpace)
{
  KeyboardState state;
  auto const out
      = pressKey (state, { KeyAction::Character, ' ', 6 }, false);
  ASSERT_TRUE (out.key.has_value ());
  EXPECT_TRUE (out.key->isKeyCode (juce::KeyPress::spaceKey));
  EXPECT_EQ (out.key->getTextCharacter (), ' ');
}

TEST (BarKeyboardModel, HideSendsNothingAndAsksToHide)
{
  KeyboardState state;
  auto const out = pressKey (state, action (KeyAction::Hide), false);
  EXPECT_FALSE (out.key.has_value ());
  EXPECT_TRUE (out.hide);
}

TEST (BarKeyboardModel, ThePageKeyFlipsThePageAndDropsShift)
{
  KeyboardState state;
  pressKey (state, action (KeyAction::Shift), false);
  auto const out = pressKey (state, action (KeyAction::Page), false);
  EXPECT_FALSE (out.key.has_value ());
  EXPECT_EQ (state.page, KeyboardPage::Symbols);
  EXPECT_EQ (state.shift, ShiftState::Off);
  pressKey (state, action (KeyAction::Page), false);
  EXPECT_EQ (state.page, KeyboardPage::Letters);
}

TEST (BarKeyboardModel, OnlyTheCursorAndBackspaceRepeatWhileHeld)
{
  EXPECT_TRUE (keyRepeatsWhileHeld (KeyAction::Backspace));
  EXPECT_TRUE (keyRepeatsWhileHeld (KeyAction::Left));
  EXPECT_TRUE (keyRepeatsWhileHeld (KeyAction::Right));
  EXPECT_FALSE (keyRepeatsWhileHeld (KeyAction::Enter));
  EXPECT_FALSE (keyRepeatsWhileHeld (KeyAction::Character));
  EXPECT_FALSE (keyRepeatsWhileHeld (KeyAction::Hide));
}

TEST (BarKeyboardModel, EncodersTypeOnlyWhileTheKeyboardIsUpAndShiftIsNot)
{
  EXPECT_TRUE (encodersDriveKeyboard (true, false));
  // Shift keeps FREQ and Q under the hand even while typing.
  EXPECT_FALSE (encodersDriveKeyboard (true, true));
  EXPECT_FALSE (encodersDriveKeyboard (false, false));
}

TEST (BarKeyboardModel, AnEncoderWalksItsFieldAndWraps)
{
  EXPECT_EQ (steppedKeyInBlock (6, 0, 1), 1);
  EXPECT_EQ (steppedKeyInBlock (6, 5, 1), 0);
  EXPECT_EQ (steppedKeyInBlock (6, 0, -1), 5);
  EXPECT_EQ (steppedKeyInBlock (6, 2, 3), 5);
  EXPECT_EQ (steppedKeyInBlock (4, 1, -6), 3);
  EXPECT_EQ (steppedKeyInBlock (0, 0, 1), 0);
}
