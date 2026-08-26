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

#include "TextInput.hh"

namespace a3
{

// The blank comes first everywhere, so turning down past 'a' shortens the
// text rather than wrapping round to a digit.
juce::String const TextInput::nameAlphabet
    = " abcdefghijklmnopqrstuvwxyz0123456789-";
juce::String const TextInput::hostAlphabet
    = " abcdefghijklmnopqrstuvwxyz0123456789-.";
juce::String const TextInput::pathAlphabet
    = " abcdefghijklmnopqrstuvwxyz0123456789-./_";

TextInput::TextInput (juce::String const &text, juce::String const &alphabet)
    : _alphabet (alphabet)
{
  _buffer = text.substring (0, maxLength);
  _buffer = _buffer.paddedRight (' ', maxLength);
}

juce::String
TextInput::name () const
{
  return _buffer.upToFirstOccurrenceOf (" ", false, false);
}

void
TextInput::moveCursor (int delta)
{
  _cursor = juce::jlimit (0, maxLength - 1, _cursor + delta);
}

juce::juce_wchar
TextInput::characterAtCursor () const
{
  return _buffer[juce::jlimit (0, maxLength - 1, _cursor)];
}

void
TextInput::changeCharacter (int delta)
{
  auto const current = _alphabet.indexOfChar (characterAtCursor ());
  auto const index
      = juce::jlimit (0, _alphabet.length () - 1,
                      (current < 0 ? 0 : current) + delta);

  _buffer = _buffer.replaceSection (_cursor, 1,
                                    juce::String::charToString (
                                        _alphabet[index]));
}


void
TextInput::type (juce::juce_wchar character)
{
  if (_alphabet.indexOfChar (character) <= 0)
    return; // not in the alphabet, or the blank — which backspace() is for

  if (_cursor >= maxLength)
    return;

  _buffer = _buffer.replaceSection (_cursor, 1,
                                    juce::String::charToString (character));
  _cursor = juce::jmin (maxLength, _cursor + 1);
}

void
TextInput::backspace ()
{
  if (_cursor <= 0)
    return;

  --_cursor;
  _buffer = _buffer.replaceSection (_cursor, 1, " ");
}

}
