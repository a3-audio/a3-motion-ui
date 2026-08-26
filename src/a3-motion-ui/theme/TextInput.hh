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

#include <JuceHeader.h>

#include <a3-motion-ui/theme/Theme.hh>

namespace a3
{

/** A short piece of text being typed — a skin's name, a host, a path.
 *
 *  Turning moves along the name, pressing arms a position, turning then
 *  walks its letter through the alphabet — the same two-level rhythm the
 *  rest of this menu uses, because there is only ever one control.
 *
 *  Each entry carries the alphabet it accepts, and that is the guard: the
 *  keyboard offers a superset — a host needs dots, a path needs slashes —
 *  and an entry silently ignores a character it may not hold, so a skin
 *  name cannot pick up a slash from the same keyboard.
 *
 *  The blank comes first in every alphabet: turning a letter down past 'a'
 *  blanks it and the text ends there, which is how it gets shortened with
 *  the same one control that lengthens it. */
class TextInput
{
public:
  static constexpr int maxLength = 24;

  /** What a skin's name may hold. */
  static juce::String const nameAlphabet;
  /** What a host or a path may hold, on top of that. */
  static juce::String const hostAlphabet;
  static juce::String const pathAlphabet;

  explicit TextInput (juce::String const &text = {},
                      juce::String const &alphabet = nameAlphabet);

  /** The name as it stands: everything up to the first blank. */
  juce::String name () const;

  int cursor () const { return _cursor; }
  void moveCursor (int delta);

  juce::juce_wchar characterAtCursor () const;
  void changeCharacter (int delta);

  /** Type a character at the cursor and move on — the touchscreen's way in.
   *  A character a name may not hold is ignored. */
  void type (juce::juce_wchar character);

  /** Take the character before the cursor. */
  void backspace ();

  /** The whole buffer, blanks included, for drawing the row. */
  juce::String buffer () const { return _buffer; }

private:
  juce::String _buffer;
  juce::String _alphabet;
  int _cursor = 0;
};

}
