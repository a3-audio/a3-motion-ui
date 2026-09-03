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

#include <a3-motion-ui/io/FunctionKeys.hh>

namespace a3
{

/** Everything a function key's look depends on, in one place so that the two
 *  displays cannot be given different halves of it. */
struct FunctionKeyLook
{
  int clockMode = 0;
  bool recording = false;
  bool shiftHeld = false;
  /** A finger is on TAP, or a beat just went by. */
  bool tapPressed = false;
  bool tapBeat = false;
};

/** The colour a function key carries, or a transparent colour for one that
 *  carries none and shows the quiet resting look instead.
 *
 *  One rule for the screen and the panel. What differs between them is not
 *  *which* colour but how loudly it is said: the screen washes it into a
 *  button face, the panel lights the key outright, because an LED in a dark
 *  booth is about as loud at full as a wash is on a lit screen.
 *
 *  TAP is the exception and deliberately so. On the panel its beat is the
 *  key's colour; on the screen it is a colourless wash, because a coloured
 *  flash on a lit screen at every single beat is the loudness that had this
 *  blink removed once already.
 */
juce::Colour functionKeyColour (FunctionKey key, FunctionKeyLook const &look);

}
