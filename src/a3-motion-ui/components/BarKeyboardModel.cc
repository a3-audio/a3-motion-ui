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

#include "BarKeyboardModel.hh"

namespace a3
{

namespace
{

ShiftState
nextShift (ShiftState shift)
{
  switch (shift)
    {
    case ShiftState::Off: return ShiftState::Once;
    case ShiftState::Once: return ShiftState::Locked;
    case ShiftState::Locked: return ShiftState::Off;
    }
  __builtin_unreachable ();
}

KeyboardPage
otherPage (KeyboardPage page)
{
  switch (page)
    {
    case KeyboardPage::Letters: return KeyboardPage::Symbols;
    case KeyboardPage::Symbols: return KeyboardPage::Letters;
    }
  __builtin_unreachable ();
}

KeyOutcome
deliver (int keyCode)
{
  return { juce::KeyPress (keyCode), false };
}

KeyOutcome
typeCharacter (KeyboardState &state, juce::juce_wchar character, bool shiftHeld)
{
  auto const upper = shiftHeld || state.shift != ShiftState::Off;
  auto const typed
      = upper ? upperCaseOf (character) : character;

  if (state.shift == ShiftState::Once)
    state.shift = ShiftState::Off;

  // The key code a plugged-in keyboard would send with it: the space key for
  // a space (the skin editor and JUCE's editors ask for that one by code),
  // and the character itself otherwise, as JUCE does on Linux.
  auto const keyCode = typed == ' ' ? juce::KeyPress::spaceKey
                                    : static_cast<int> (typed);
  return { juce::KeyPress (keyCode, juce::ModifierKeys (), typed), false };
}

}

KeyOutcome
pressKey (KeyboardState &state, KeyDef const &key, bool shiftHeld)
{
  switch (key.action)
    {
    case KeyAction::Character:
      return typeCharacter (state, key.character, shiftHeld);
    case KeyAction::Backspace: return deliver (juce::KeyPress::backspaceKey);
    case KeyAction::Enter: return deliver (juce::KeyPress::returnKey);
    case KeyAction::Left: return deliver (juce::KeyPress::leftKey);
    case KeyAction::Right: return deliver (juce::KeyPress::rightKey);
    case KeyAction::Escape: return deliver (juce::KeyPress::escapeKey);
    case KeyAction::Shift:
      state.shift = nextShift (state.shift);
      return {};
    case KeyAction::Page:
      state.page = otherPage (state.page);
      state.shift = ShiftState::Off;
      return {};
    case KeyAction::Hide: return { std::nullopt, true };
    }
  __builtin_unreachable ();
}

bool
keyRepeatsWhileHeld (KeyAction action)
{
  switch (action)
    {
    case KeyAction::Backspace:
    case KeyAction::Left:
    case KeyAction::Right: return true;
    case KeyAction::Character:
    case KeyAction::Enter:
    case KeyAction::Shift:
    case KeyAction::Page:
    case KeyAction::Escape:
    case KeyAction::Hide: return false;
    }
  __builtin_unreachable ();
}

bool
encodersDriveKeyboard (bool keyboardShown, bool shiftHeld)
{
  return keyboardShown && !shiftHeld;
}

int
steppedKeyInBlock (int count, int current, int increment)
{
  if (count <= 0)
    return 0;
  auto const stepped = (current + increment) % count;
  return stepped < 0 ? stepped + count : stepped;
}

}
