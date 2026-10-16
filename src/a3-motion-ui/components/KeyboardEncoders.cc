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
  // Before anything else: SHIFT + encoder is FREQ / Q on every page.
  if (shiftHeld)
    return KeyboardEncoderJob::ChannelPot;
  if (column == 0)
    return row == 0 ? KeyboardEncoderJob::CursorVertical
                    : KeyboardEncoderJob::Folder;
  if (column == 1)
    return row == 0 ? KeyboardEncoderJob::CursorHorizontal
                    : KeyboardEncoderJob::File;
  return KeyboardEncoderJob::Nothing;
}

juce::KeyPress
cursorKeyOfDetent (KeyboardEncoderJob job, int increment)
{
  auto const forward = increment > 0;
  if (job == KeyboardEncoderJob::CursorVertical)
    return juce::KeyPress (forward ? juce::KeyPress::downKey
                                   : juce::KeyPress::upKey);
  return juce::KeyPress (forward ? juce::KeyPress::rightKey
                                 : juce::KeyPress::leftKey);
}

KnobGlyph
keyboardEncoderGlyph (KeyboardEncoderJob job)
{
  switch (job)
    {
    case KeyboardEncoderJob::CursorVertical: return KnobGlyph::UpDown;
    case KeyboardEncoderJob::CursorHorizontal: return KnobGlyph::LeftRight;
    case KeyboardEncoderJob::Folder: return KnobGlyph::Folder;
    case KeyboardEncoderJob::File: return KnobGlyph::File;
    case KeyboardEncoderJob::PageControl:
    case KeyboardEncoderJob::ChannelPot:
    case KeyboardEncoderJob::Nothing: return KnobGlyph::None;
    }
  return KnobGlyph::None;
}

}
