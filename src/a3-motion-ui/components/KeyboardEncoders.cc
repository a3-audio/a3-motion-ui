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

#include "KeyboardEncoders.hh"

namespace a3
{

KeyboardEncoderJob
keyboardEncoderJob (bool keyboardShown, int column, int row, bool shiftHeld)
{
  if (!keyboardShown)
    return KeyboardEncoderJob::PageControl;
  // Before the cursor: SHIFT + encoder is FREQ / Q on every page.
  if (shiftHeld)
    return KeyboardEncoderJob::ChannelPot;
  if (column == 0 && row == 0)
    return KeyboardEncoderJob::Cursor;
  if (column == 0 && row == 1)
    return KeyboardEncoderJob::ScrollLines;
  if (column == 1 && row == 1)
    return KeyboardEncoderJob::ScrollColumns;
  return KeyboardEncoderJob::Nothing;
}

int
keyboardScrollStep (KeyboardEncoderJob job, int increment)
{
  switch (job)
    {
    case KeyboardEncoderJob::ScrollLines: return increment * linesPerDetent;
    case KeyboardEncoderJob::ScrollColumns:
      return increment * columnsPerDetent;
    case KeyboardEncoderJob::PageControl:
    case KeyboardEncoderJob::ChannelPot:
    case KeyboardEncoderJob::Cursor:
    case KeyboardEncoderJob::Nothing: return 0;
    }
  return 0;
}

juce::KeyPress
cursorKeyOfDetent (int increment)
{
  return juce::KeyPress (increment > 0 ? juce::KeyPress::rightKey
                                       : juce::KeyPress::leftKey);
}

KnobGlyph
keyboardEncoderGlyph (KeyboardEncoderJob job)
{
  switch (job)
    {
    case KeyboardEncoderJob::Cursor: return KnobGlyph::Cursor;
    case KeyboardEncoderJob::ScrollLines: return KnobGlyph::ScrollVertical;
    case KeyboardEncoderJob::ScrollColumns: return KnobGlyph::ScrollHorizontal;
    case KeyboardEncoderJob::PageControl:
    case KeyboardEncoderJob::ChannelPot:
    case KeyboardEncoderJob::Nothing: return KnobGlyph::None;
    }
  return KnobGlyph::None;
}

}
