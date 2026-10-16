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

#pragma once

#include <a3-motion-ui/components/BarKeyboardLayout.hh>
#include <a3-motion-ui/io/PanelGrid.hh>

namespace a3
{
/** The keyboard is the panel (2026-09-30): each of its 44 keys is a key of
 *  the keyboard, QWERTY in the four rows that are ten wide, the corners and
 *  the bottom row for what is not a letter.
 *
 *  ```
 *          col0   1    2    3    4    5    6    7    8   col9
 *  row 0   ESC                                            BKSP
 *  row 1   123                                            ENTER
 *  row 2    q     w    e    r    t    y    u    i    o    p
 *  row 3    a     s    d    f    g    h    j    k    l    -
 *  row 4    z     x    c    v    b    n    m    ,    .    /
 *  row 5   SHIFT  <    >   SPACE SPACE SPACE SPACE '    "   HIDE
 *  ```
 *
 *  The symbols page changes rows 2-4 only. */
KeyDef panelKeyAt (KeyboardPage page, PanelCell cell);

/** The keys as the screen draws them in `area`: one per panel cell, on the
 *  PADS grid, except the four space cells, which are one bar. */
std::vector<KeyCap> layOutPanelKeyboard (juce::Rectangle<int> area,
                                         KeyboardPage page);

/** Where KEYS shows encoder (`column`, `row`) -- EncoderMap's four by two,
 *  column the channel, row 0 the upper encoder: over the channel's two pad
 *  columns in rows 0-1, which on the panel carry the pots. The knob stands in
 *  the left cell, the sign of what it does in the right (decided
 *  2026-10-16: beside, not on the knob, so the sign is twice the size). */
struct KeyboardEncoderSlot
{
  juce::Rectangle<int> knob;
  juce::Rectangle<int> glyph;
};

KeyboardEncoderSlot keyboardEncoderSlot (juce::Rectangle<int> area,
                                         int column, int row);

/** The file keys on KEYS (2026-10-16): FILES' keys in a column left of the
 *  keys, one per key row, so a file can be named, kept or loaded while the
 *  keyboard is up. Top to bottom. */
enum class FileKey
{
  Rename,
  Delete,
  Load,
  Save,
  SaveAs,
  From,
};

constexpr int numFileKeys = 6;

/** Which file key stands beside key row `row` (0-5). */
FileKey fileKeyOfRow (int row);

/** Where it stands: in the margin left of the row's first key, as tall as
 *  the key. */
juce::Rectangle<int> keyboardFileKeySlot (juce::Rectangle<int> area, int row);

/** A key's LED while the keyboard owns the panel: letters and other
 *  characters in \`letter\`, every other key in \`accent\`, no key unlit. */
juce::Colour keyboardLedColour (KeyDef const &key, juce::Colour accent,
                                juce::Colour letter);

/** How bright a letter's LED is: enough to read as "a key" in a dark booth,
 *  little enough that the accent is what the eye looks for. */
constexpr float keyboardLetterLedBrightness = 0.22f;
}
