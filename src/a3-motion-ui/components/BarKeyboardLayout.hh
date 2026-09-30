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
#include <a3-motion-ui/io/PanelGrid.hh>

#include <array>
#include <vector>

namespace a3
{

/** The in-app keyboard's keys (2026-09-28): what a key is and what it says.
 *
 *  Where they stand is the panel's since 2026-09-30 -- see PanelKeyboard.hh:
 *  the keyboard is the panel's 44 keys, QWERTY. Until then it was four rows
 *  of twelve on the encoders' fields, QWERTZ, walked by the encoders. */
enum class KeyboardPage
{
  Letters,
  /** Digits and what the scripts, clips and sets are written with. */
  Symbols,
};

enum class KeyAction
{
  /** No key: a place the panel has none. */
  None,
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



/** A key where it stands on the screen. */
struct KeyCap
{
  KeyDef def;
  juce::Rectangle<int> bounds;
  /** The panel key it is (the first, for the space bar). */
  PanelCell cell{};
};




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
