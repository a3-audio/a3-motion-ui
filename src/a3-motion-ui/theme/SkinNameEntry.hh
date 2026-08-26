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

/** A skin name being typed with one encoder.
 *
 *  Turning moves along the name, pressing arms a position, turning then
 *  walks its letter through the alphabet — the same two-level rhythm the
 *  rest of this menu uses, because there is only ever one control.
 *
 *  The alphabet holds exactly what a skin name may hold (see
 *  isUsableSkinName), plus a blank: turning a letter down past 'a' blanks
 *  it and the name ends there, which is how a name gets shortened with the
 *  same one control that lengthens it. */
class SkinNameEntry
{
public:
  static constexpr int maxLength = 16;

  explicit SkinNameEntry (juce::String const &name = {});

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
  int _cursor = 0;
};

}
