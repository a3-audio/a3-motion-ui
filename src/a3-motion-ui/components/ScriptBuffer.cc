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

#include "ScriptBuffer.hh"

namespace a3
{

ScriptBuffer::ScriptBuffer (juce::String const &text) { setText (text); }

void
ScriptBuffer::setText (juce::String const &text)
{
  // Whichever way the file was written: a script edited on a desk and carried
  // over on a stick must not arrive with a row of stray characters on it.
  _lines = juce::StringArray::fromLines (text.replace ("\r\n", "\n"));

  // fromLines gives nothing at all for an empty string, and a buffer with no
  // lines has nowhere to put a caret.
  if (_lines.isEmpty ())
    _lines.add ({});

  _caretLine = 0;
  _caretColumn = 0;
  _firstVisibleLine = 0;
  _edited = false;
}

juce::String
ScriptBuffer::text () const
{
  return _lines.joinIntoString ("\n");
}

juce::String
ScriptBuffer::line (int index) const
{
  return juce::isPositiveAndBelow (index, _lines.size ()) ? _lines[index]
                                                         : juce::String{};
}

void
ScriptBuffer::placeCaret (int line, int column)
{
  _caretLine = line;
  _caretColumn = column;
  clampCaret ();
}

void
ScriptBuffer::moveCaret (int lines, int columns)
{
  _caretLine += lines;
  _caretColumn += columns;
  clampCaret ();
}

void
ScriptBuffer::clampCaret ()
{
  _caretLine = juce::jlimit (0, _lines.size () - 1, _caretLine);
  _caretColumn
      = juce::jlimit (0, _lines[_caretLine].length (), _caretColumn);
}

void
ScriptBuffer::type (juce::juce_wchar character)
{
  auto &line = _lines.getReference (_caretLine);

  if (character == '\n' || character == '\r')
    {
      auto const tail = line.substring (_caretColumn);
      line = line.substring (0, _caretColumn);

      _lines.insert (_caretLine + 1, tail);
      ++_caretLine;
      _caretColumn = 0;
    }
  else
    {
      line = line.substring (0, _caretColumn)
             + juce::String::charToString (character)
             + line.substring (_caretColumn);
      ++_caretColumn;
    }

  _edited = true;
}

void
ScriptBuffer::backspace ()
{
  if (_caretColumn > 0)
    {
      auto &line = _lines.getReference (_caretLine);
      line = line.substring (0, _caretColumn - 1)
             + line.substring (_caretColumn);
      --_caretColumn;
      _edited = true;
      return;
    }

  if (_caretLine == 0)
    return; // the very start: nothing behind it to take

  // At the start of a line the break above it is what is behind the caret, so
  // taking it joins the two -- with the caret landing on the join, which is
  // where the next character goes.
  auto const tail = _lines[_caretLine];
  _lines.remove (_caretLine);
  --_caretLine;

  auto &above = _lines.getReference (_caretLine);
  _caretColumn = above.length ();
  above += tail;
  _edited = true;
}

void
ScriptBuffer::bringCaretIntoView (int visibleLines)
{
  if (visibleLines <= 0)
    return;

  if (_caretLine < _firstVisibleLine)
    _firstVisibleLine = _caretLine;
  else if (_caretLine >= _firstVisibleLine + visibleLines)
    _firstVisibleLine = _caretLine - visibleLines + 1;

  _firstVisibleLine
      = juce::jlimit (0, juce::jmax (0, _lines.size () - 1), _firstVisibleLine);
}

void
ScriptBuffer::scrollByDrag (int dragIncrement, int visibleLines)
{
  scrollBy (dragIncrement, visibleLines);
}

void
ScriptBuffer::scrollBy (int lines, int visibleLines)
{
  juce::ignoreUnused (visibleLines);

  _firstVisibleLine = juce::jlimit (0, juce::jmax (0, _lines.size () - 1),
                                    _firstVisibleLine + lines);
}

}
