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

#include <a3-motion-ui/components/ClipSettingsLayout.hh>

#include <array>
#include <vector>

namespace a3
{

/** The in-app keyboard's geometry and its keys (2026-09-28).
 *
 *  It stands in the bar's clip content -- the rectangle CLIP, MOTION, ACTION,
 *  CHMIX and REC use -- so the sphere, the channel row, the header and the
 *  global strip stay in view, and with them every field a text is typed
 *  into (the menu's masks and the FILES editor both lie over the sphere).
 *
 *  **It is laid out on the encoders' four by two**: the eight fields CLIP
 *  and REC are made of (`ClipSettingsLayout::pageFields`). Each field holds
 *  two rows of three keys, so the keyboard is four rows of twelve and every
 *  key stands under exactly one encoder -- the one that walks it (see
 *  BarKeyboardModel.hh). A wide key (space, hide) spans whole slots and
 *  crosses a field edge only where two fields meet.
 *
 *  QWERTZ, because the maintainer types German and a hand that knows a
 *  German keyboard finds Z, Y and the umlauts where it expects them. */

enum class KeyboardPage
{
  Letters,
  /** Digits and what the scripts, clips and sets are written with. */
  Symbols,
};

enum class KeyAction
{
  Character,
  Backspace,
  Enter,
  Shift,
  /** Letters <-> symbols. */
  Page,
  Left,
  Right,
  Escape,
  /** Puts the keyboard away; the field stays open. */
  Hide,
};

/** One key: what it does, the character it types (lower case; Shift makes
 *  the upper), and how many of the row's twelve slots it takes. */
struct KeyDef
{
  KeyAction action = KeyAction::Character;
  juce::juce_wchar character = 0;
  int span = 1;
};

constexpr int keyboardRows = 4;
constexpr int keyboardSlotsPerColumn = 3;
constexpr int keyboardSlotsPerRow = keyboardSlotsPerColumn * 4;

/** A page's row, left to right. */
std::vector<KeyDef> const &keyboardRow (KeyboardPage page, int row);

/** A key where it stands. `row` 0..3 from the top, `firstSlot` 0..11. */
struct KeyCap
{
  KeyDef def;
  juce::Rectangle<int> bounds;
  int row = 0;
  int firstSlot = 0;
};

/** The eight fields the keyboard is laid into: the bar's page fields,
 *  whichever page is shown -- ACTION and CHMIX lay out none of their own. */
std::array<juce::Rectangle<int>, 8>
keyboardFieldsOf (ClipSettingsLayout const &bar);

/** Every key of a page, in reading order. The gap between keys is the gap
 *  between the fields, so the keyboard keeps the bar's rhythm. */
std::vector<KeyCap>
layOutBarKeyboard (std::array<juce::Rectangle<int>, 8> const &fields,
                   KeyboardPage page);

/** Which keys stand in field `block` (row * 4 + column, as the encoders are
 *  numbered), as indices into `keys`, in reading order. A wide key belongs
 *  to every field it reaches into. */
std::vector<size_t> keysOfBlock (std::vector<KeyCap> const &keys, int block);

/** The key under `point`, or -1. */
int keyAt (std::vector<KeyCap> const &keys, juce::Point<int> point);

/** The capital of a key's character. The umlauts explicitly: the C
 *  library's towupper only knows them under a locale that says so, and the
 *  device's may not. ß has no capital a name would use and stays. */
juce::juce_wchar upperCaseOf (juce::juce_wchar character);

/** What a key says on its face. Empty for the two cursor keys, which are
 *  drawn as arrows. */
juce::String keyLabel (KeyDef const &key, KeyboardPage page, bool upper);

}
