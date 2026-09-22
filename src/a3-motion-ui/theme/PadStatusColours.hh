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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

namespace a3
{

/** What a pad looks like for what the slot under it is doing.
 *
 *  One rule, read by two displays: the panel's LEDs and the pads page on
 *  screen. That was already true -- this only moves the decision where the
 *  other look rules live, beside TransportLook and FunctionKeyColours, and
 *  puts its three shades in the skin instead of in three literals.
 *
 *  The scale is subtraction from the channel's own colour, and full is the
 *  top of it: a pad that is doing something is undimmed, because what the eye
 *  finds across a dark booth is the pad that is fully lit. Below it the shades
 *  say how little is there -- an empty slot least, an idle one holding a clip,
 *  and between them the dark half of a blink, which is on its way back up.
 *
 *  `step` is the blink's phase, counted in pad-LED ticks. `statusLast` and
 *  `oneShotRecording` answer one question between them: a one-shot take
 *  coming to rest is not waiting for a beat, so it must not blink as though
 *  it were.
 */
/** The colour a pad starts from, before its slot's status shades it.
 *
 *  Play turns `accent` while its clip runs. The Action pad turns white -- the
 *  skin's text colour, so its mark goes black -- for exactly as long as the
 *  action runs (rise, hold and fall), on every channel: a pressed ACT used to
 *  look like a Play and said nothing about how long it would last, and a
 *  yellow highlight vanished on a yellow channel. Everything else wears its
 *  channel. */
juce::Colour padBaseColour (PadFunction function, bool clipPlaying,
                            bool actionRunning, juce::Colour channel);

juce::Colour padStatusColour (juce::Colour base, Pattern::Status status,
                              Pattern::Status statusLast, int step,
                              bool oneShotRecording);

}
