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

#pragma once

#include <JuceHeader.h>

#include <a3-motion-ui/components/BarKeyboardLayout.hh>

#include <optional>

namespace a3
{

/** What a key press on the in-app keyboard means -- no window, no drawing.
 *
 *  The keyboard types nothing itself. A key becomes a `juce::KeyPress`, the
 *  same event a plugged-in keyboard produces, and goes to whatever holds the
 *  focus: the skin editor's masks, the FILES rename row and the script
 *  editor already know what Backspace, Enter, Escape and the arrows mean
 *  there, and a second copy of that here would be a second truth. */

enum class ShiftState
{
  Off,
  /** The next letter only. */
  Once,
  Locked,
};

struct KeyboardState
{
  KeyboardPage page = KeyboardPage::Letters;
  ShiftState shift = ShiftState::Off;
};

struct KeyOutcome
{
  /** The key to deliver, if any. */
  std::optional<juce::KeyPress> key;
  /** Put the keyboard away. */
  bool hide = false;
};

/** Presses `key`. `shiftHeld` is SHIFT held down -- the panel's or the
 *  screen's -- which capitalises like a real keyboard's and latches nothing. */
KeyOutcome pressKey (KeyboardState &state, KeyDef const &key, bool shiftHeld);

/** Backspace and the cursor go on while held; nothing else does. */
bool keyRepeatsWhileHeld (KeyAction action);

/** The panel while the keyboard is up (one table, 2026-09-28):
 *
 *  | Panel                    | While typing                                  |
 *  |--------------------------|-----------------------------------------------|
 *  | encoder, turned          | walks the six keys of the field it stands     |
 *  |                          | under (upper: key rows 1-2, lower: 3-4)       |
 *  | encoder, pressed         | types the key it stands on                    |
 *  | SHIFT + encoder          | FREQ / Q of its channel, as always            |
 *  | SHIFT held + a key       | capital letter                                |
 *  | pads, pots, TAP, clock,  | unchanged -- the set goes on while a name is  |
 *  | REC, recmode, MENU       | typed; MENU still goes back one level         |
 *
 *  Why no pad is Enter or Backspace: the pads play the set, and the keyboard
 *  is opened mid-set. A pad that types instead of firing, for as long as a
 *  mask happens to be open, is a clip that did not start. */
bool encodersDriveKeyboard (bool keyboardShown, bool shiftHeld);

/** An encoder's walk through the `count` keys of its field: wraps both
 *  ways, so no key is more than half a field's turn away. */
int steppedKeyInBlock (int count, int current, int increment);

}
