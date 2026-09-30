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

#include "PanelKeyboard.hh"

#include <JuceHeader.h>

namespace a3
{

namespace
{
KeyDef
character (juce::juce_wchar c)
{
  return { KeyAction::Character, c, 1 };
}

KeyDef
action (KeyAction a)
{
  return { a, 0, 1 };
}

/** Rows 2-4, ten wide, per page. */
juce::String
typedRow (KeyboardPage page, int row)
{
  static char const *const letters[] = { "qwertyuiop", "asdfghjkl-",
                                         "zxcvbnm,./" };
  static char const *const symbols[] = { "1234567890", "-/:;()=+_*",
                                         "{}[]<>\\|!?" };
  auto const index = static_cast<std::size_t> (row - firstPanelPadRow);
  return page == KeyboardPage::Letters ? letters[index] : symbols[index];
}

KeyDef
bottomRowKey (int col)
{
  switch (col)
    {
    case 0: return action (KeyAction::Shift);
    case 1: return action (KeyAction::Left);
    case 2: return action (KeyAction::Right);
    case 7: return character ('\'');
    case 8: return character ('"');
    case 9: return action (KeyAction::Hide);
    default: return character (' ');
    }
}
}

KeyDef
panelKeyAt (KeyboardPage page, PanelCell cell)
{
  if (!isPanelCell (cell))
    return action (KeyAction::None);

  auto const rightEnd = cell.col == panelColumns - 1;
  if (cell.row == 0)
    return action (rightEnd ? KeyAction::Backspace : KeyAction::Escape);
  if (cell.row == 1)
    return action (rightEnd ? KeyAction::Enter : KeyAction::Page);
  if (cell.row == panelRows - 1)
    return bottomRowKey (cell.col);

  return character (typedRow (page, cell.row)[cell.col]);
}

}
