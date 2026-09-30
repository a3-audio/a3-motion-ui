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
    case KeyAction::None: return {};
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
