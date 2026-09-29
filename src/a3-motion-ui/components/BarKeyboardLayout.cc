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

#include "BarKeyboardLayout.hh"

namespace a3
{

namespace
{

KeyDef
typing (juce::juce_wchar character, int span = 1)
{
  return { KeyAction::Character, character, span };
}

KeyDef
editing (KeyAction action, int span = 1)
{
  return { action, 0, span };
}

/** A row of single-slot characters, written as the row reads. */
std::vector<KeyDef>
characters (juce::String const &text)
{
  std::vector<KeyDef> row;
  for (auto character : text)
    row.push_back (typing (character));
  return row;
}

std::vector<KeyDef>
joined (std::vector<KeyDef> row, std::vector<KeyDef> const &tail)
{
  row.insert (row.end (), tail.begin (), tail.end ());
  return row;
}

juce::String
utf8 (char const *text)
{
  return juce::String (juce::CharPointer_UTF8 (text));
}

/** The bottom row, the same on both pages: the page key and the cursor
 *  under the left field, space across the two middle ones, Escape and Hide
 *  under the right one -- Escape a slot, Hide two, because putting the
 *  keyboard away is the one key here pressed without aiming. */
std::vector<KeyDef>
bottomRow ()
{
  return { editing (KeyAction::Page),   editing (KeyAction::Left),
           editing (KeyAction::Right),  typing (' ', 6),
           editing (KeyAction::Escape), editing (KeyAction::Hide, 2) };
}

std::array<std::vector<KeyDef>, keyboardRows> const &
lettersPage ()
{
  // QWERTZ, the umlauts where a German keyboard has them, Backspace over
  // Enter at the right edge, and the three characters a name is made of
  // besides letters and digits (- _ .) at the end of the third row.
  static std::array<std::vector<KeyDef>, keyboardRows> const rows{
    joined (characters (utf8 ("qwertzuiop\xc3\xbc")),
            { editing (KeyAction::Backspace) }),
    joined (characters (utf8 ("asdfghjkl\xc3\xb6\xc3\xa4")),
            { editing (KeyAction::Enter) }),
    joined ({ editing (KeyAction::Shift) },
            characters (utf8 ("yxcvbnm\xc3\x9f.-_"))),
    bottomRow (),
  };
  return rows;
}

std::array<std::vector<KeyDef>, keyboardRows> const &
symbolsPage ()
{
  // Digits across the top as on any keyboard, then what the shipped scripts,
  // clips and sets are written with, most frequent first. Backspace and Enter
  // stay where they are on the letters page.
  static std::array<std::vector<KeyDef>, keyboardRows> const rows{
    joined (characters ("1234567890."), { editing (KeyAction::Backspace) }),
    joined (characters ("-/\":;=~\\',+"), { editing (KeyAction::Enter) }),
    characters ("(){}[]<>*_|!"),
    bottomRow (),
  };
  return rows;
}

/** A field's slot: two rows of three, the field's own gap between them. */
juce::Rectangle<int>
slotIn (juce::Rectangle<int> field, int rowInField, int column, int gap)
{
  auto const rowH = (field.getHeight () - gap) / 2;
  auto const y = field.getY () + rowInField * (rowH + gap);
  // Every edge from the field's whole width, so the remainder is spread
  // rather than piled against the right edge (as spreadKeys does).
  auto const available = field.getWidth () - gap * (keyboardSlotsPerColumn - 1);
  auto const x0 = field.getX () + column * gap
                  + (available * column) / keyboardSlotsPerColumn;
  auto const x1 = field.getX () + column * gap
                  + (available * (column + 1)) / keyboardSlotsPerColumn;
  return { x0, y, x1 - x0, rowH };
}

int
fieldGap (std::array<juce::Rectangle<int>, 8> const &fields)
{
  return juce::jmax (0, fields[1].getX () - fields[0].getRight ());
}

}

std::vector<KeyDef> const &
keyboardRow (KeyboardPage page, int row)
{
  auto const index = static_cast<size_t> (juce::jlimit (0, keyboardRows - 1, row));
  switch (page)
    {
    case KeyboardPage::Letters: return lettersPage ()[index];
    case KeyboardPage::Symbols: return symbolsPage ()[index];
    }
  __builtin_unreachable ();
}

std::array<juce::Rectangle<int>, 8>
keyboardFieldsOf (ClipSettingsLayout const &bar)
{
  // The fields CLIP lays out, whichever page is up: a page without fields of
  // its own (ACTION, CHMIX) leaves pageFields empty.
  return pageFieldGrid (bar);
}

std::vector<KeyCap>
layOutBarKeyboard (std::array<juce::Rectangle<int>, 8> const &fields,
                   KeyboardPage page)
{
  auto const gap = fieldGap (fields);
  std::vector<KeyCap> keys;
  for (int row = 0; row < keyboardRows; ++row)
    {
      int slot = 0;
      for (auto const &def : keyboardRow (page, row))
        {
          auto const slotRect = [&] (int s) {
            auto const column = s / keyboardSlotsPerColumn;
            auto const &field
                = fields[static_cast<size_t> ((row / 2) * 4 + column)];
            return slotIn (field, row % 2, s % keyboardSlotsPerColumn, gap);
          };
          auto const first = slotRect (slot);
          auto const last = slotRect (slot + def.span - 1);
          keys.push_back ({ def, first.getUnion (last), row, slot });
          slot += def.span;
        }
    }
  return keys;
}

std::vector<size_t>
keysOfBlock (std::vector<KeyCap> const &keys, int block)
{
  auto const fieldRow = block / 4;
  auto const column = block % 4;
  auto const firstSlot = column * keyboardSlotsPerColumn;
  auto const endSlot = firstSlot + keyboardSlotsPerColumn;

  std::vector<size_t> inBlock;
  for (size_t i = 0; i < keys.size (); ++i)
    {
      auto const &key = keys[i];
      if (key.row / 2 != fieldRow)
        continue;
      auto const keyEnd = key.firstSlot + key.def.span;
      if (key.firstSlot < endSlot && keyEnd > firstSlot)
        inBlock.push_back (i);
    }
  return inBlock;
}

int
keyAt (std::vector<KeyCap> const &keys, juce::Point<int> point)
{
  for (size_t i = 0; i < keys.size (); ++i)
    if (keys[i].bounds.contains (point))
      return static_cast<int> (i);
  return -1;
}

juce::juce_wchar
upperCaseOf (juce::juce_wchar character)
{
  switch (character)
    {
    case 0x00E4: return 0x00C4; // ä
    case 0x00F6: return 0x00D6; // ö
    case 0x00FC: return 0x00DC; // ü
    case 0x00DF: return 0x00DF; // ß
    default: break;
    }
  return juce::CharacterFunctions::toUpperCase (character);
}

juce::String
keyLabel (KeyDef const &key, KeyboardPage page, bool upper)
{
  switch (key.action)
    {
    case KeyAction::Character:
      if (key.character == ' ')
        return "SPACE";
      return juce::String::charToString (upper ? upperCaseOf (key.character)
                                               : key.character);
    case KeyAction::Backspace: return "DEL";
    case KeyAction::Enter: return "ENTER";
    case KeyAction::Shift: return "SHIFT";
    case KeyAction::Page:
      return page == KeyboardPage::Letters ? "123" : "ABC";
    case KeyAction::Left:
    case KeyAction::Right: return {};
    case KeyAction::Escape: return "ESC";
    case KeyAction::Hide: return "HIDE";
    }
  __builtin_unreachable ();
}

}
