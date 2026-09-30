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

/** The panel while the keyboard is up (since 2026-09-30):
 *
 *  | Panel                    | While typing                                  |
 *  |--------------------------|-----------------------------------------------|
 *  | the 44 keys              | the keyboard's keys (PanelKeyboard.hh); no    |
 *  |                          | clip fires, TAP/REC/MENU/SHIFT do nothing     |
 *  |                          | else. HIDE or ESC gives the panel back.       |
 *  | encoder 1, turned        | the text cursor, left or right                |
 *  | the other encoders, pots | unchanged -- the mix stays under the hands    |
 *
 *  Until then the pads kept playing while a name was typed, on purpose ("a
 *  pad that types instead of firing ... is a clip that did not start"). The
 *  maintainer asked for the opposite: "den hardware controller komplett
 *  übernehmen wenn aktiviert" -- running clips go on, new ones wait. */

/** While the keyboard is up, encoder 1 (column 0, the upper row) moves the
 *  text cursor, one key press per detent in the turn's direction -- the
 *  other encoders keep their own jobs. Nothing for any other encoder, or
 *  with the keyboard down. */
std::optional<juce::KeyPress> cursorKeyOfEncoder (bool keyboardShown,
                                                  int column, int row,
                                                  int increment);

}
