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

#include "SkinNameEntry.hh"

namespace a3
{

namespace
{
// The blank comes first, so turning down past 'a' shortens the name rather
// than wrapping round to a digit.
juce::String const alphabet = " abcdefghijklmnopqrstuvwxyz0123456789-";
}

SkinNameEntry::SkinNameEntry (juce::String const &name)
{
  _buffer = name.substring (0, maxLength);
  _buffer = _buffer.paddedRight (' ', maxLength);
}

juce::String
SkinNameEntry::name () const
{
  return _buffer.upToFirstOccurrenceOf (" ", false, false);
}

void
SkinNameEntry::moveCursor (int delta)
{
  _cursor = juce::jlimit (0, maxLength - 1, _cursor + delta);
}

juce::juce_wchar
SkinNameEntry::characterAtCursor () const
{
  return _buffer[_cursor];
}

void
SkinNameEntry::changeCharacter (int delta)
{
  auto const current = alphabet.indexOfChar (characterAtCursor ());
  auto const index
      = juce::jlimit (0, alphabet.length () - 1,
                      (current < 0 ? 0 : current) + delta);

  _buffer = _buffer.replaceSection (_cursor, 1,
                                    juce::String::charToString (
                                        alphabet[index]));
}

}
