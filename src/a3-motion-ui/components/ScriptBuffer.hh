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

namespace a3

{

/** Lines of text with a caret in them, and nothing else.
 *
 *  Its own thing rather than a juce::TextEditor because what this holds is a
 *  *script*, and the page around it has to know which line the caret is on to
 *  draw it, which line the window starts at to scroll it, and whether it has
 *  been touched since it was last written. All three are questions about the
 *  text, so they belong with the text — and all three can then be checked
 *  without a window on screen.
 *
 *  Not TextInput either: that one holds a name, guards it with an alphabet
 *  and ends it at the first blank. A script is many lines and takes any
 *  character; a script that dropped a bracket because it was not in an
 *  alphabet would be a script that could not be written.
 */
class ScriptBuffer
{
public:
  explicit ScriptBuffer (juce::String const &text = {});

  /** Replace everything. Not an edit — this is loading, and a buffer that
   *  called it one would offer to save a file back over itself. */
  void setText (juce::String const &text);
  juce::String text () const;

  int numLines () const { return _lines.size (); }
  juce::String line (int index) const;

  int caretLine () const { return _caretLine; }
  int caretColumn () const { return _caretColumn; }

  /** Somewhere in the text, clamped to it — what a tap in the page means. */
  void placeCaret (int line, int column);
  /** By whole lines and columns, clamped. Landing on a shorter line lands on
   *  that line's end rather than in mid-air. */
  void moveCaret (int lines, int columns);

  /** One character at the caret. A newline splits the line there. */
  void type (juce::juce_wchar character);
  /** The character before the caret; at the start of a line, the line break
   *  above it, which joins the two. */
  void backspace ();

  /** The first line the page draws. */
  int firstVisibleLine () const { return _firstVisibleLine; }
  /** Move the window as little as it takes to show the caret. Scrolling to
   *  put the caret in the middle would jump the text under a finger that is
   *  typing, which is how you lose your place. */
  void bringCaretIntoView (int visibleLines);
  /** A drag, in lines. */
  void scrollBy (int lines, int visibleLines);

  /** Whether anything has been typed since the last markSaved(). */
  bool isEdited () const { return _edited; }
  void markSaved () { _edited = false; }

private:
  void clampCaret ();

  juce::StringArray _lines;
  int _caretLine = 0;
  int _caretColumn = 0;
  int _firstVisibleLine = 0;
  bool _edited = false;
};

}
