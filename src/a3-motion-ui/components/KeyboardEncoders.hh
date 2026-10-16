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

#include <a3-motion-ui/components/KnobGlyph.hh>

namespace a3
{

/** What an encoder does while the keyboard is up (KEYS, decided
 *  2026-10-16, second round) -- no window, no drawing.
 *
 *  | Encoder         | Turn                                                 |
 *  |-----------------|------------------------------------------------------|
 *  | E1 (col 0, up)  | the text cursor up / down                            |
 *  | E2 (col 1, up)  | the text cursor left / right                         |
 *  | E5 (col 0, low) | FILES' folder: SETS, CLIPS, SVG, ACTIONS             |
 *  | E6 (col 1, low) | the file chosen in FILES' list                       |
 *  | E3, E4, E7, E8  | nothing -- drawn dimmed                              |
 *  | any + SHIFT     | FREQ / Q of its channel, as on every page            |
 *
 *  Moving the cursor scrolls the editor to keep it in view, so the view
 *  needs no knobs of its own. The other encoders used to keep turning the
 *  hidden page's fields while the keyboard covered it: a knob that changes
 *  something nobody can see is the worst kind in a booth. Presses do nothing
 *  while the keyboard is up, for the same reason. */
enum class KeyboardEncoderJob
{
  /** The keyboard is down: whatever the page says. */
  PageControl,
  /** SHIFT held: FREQ / Q of the encoder's own channel. */
  ChannelPot,
  CursorVertical,
  CursorHorizontal,
  Folder,
  File,
  Nothing,
};

/** `column` is the channel (0-3), `row` 0 the upper encoder, 1 the lower --
 *  EncoderMap's four by two. SHIFT is asked before anything else. */
KeyboardEncoderJob keyboardEncoderJob (bool keyboardShown, int column,
                                       int row, bool shiftHeld);

/** The key one detent of a cursor encoder types: clockwise is down or
 *  right. */
juce::KeyPress cursorKeyOfDetent (KeyboardEncoderJob job, int increment);

/** The sign drawn beside a KEYS knob for what it does. */
KnobGlyph keyboardEncoderGlyph (KeyboardEncoderJob job);

}
