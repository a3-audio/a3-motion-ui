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
/** How far apart two fills are, as the straight-line distance between them in
 *  RGB (0..441). Not contrast: a lit pad differs from its channel by *hue* as
 *  often as by brightness -- the highlight on a blue pad has a contrast ratio
 *  near 1 and is obvious -- so a luminance ratio would call the wrong ones
 *  alike. */
float colourDistance (juce::Colour a, juce::Colour b);

/** The least distance at which a lit pad reads as changed. The highlight on
 *  the shipped yellow channel is 12 apart, on every other channel over 200. */
constexpr float minimumFillDistance = 100.f;

/** The colour a pad starts from, before its slot's status shades it.
 *
 *  Play turns `accent` while its clip runs; the Action pad turns its own
 *  colour (`highlight`) for exactly as long as the action runs -- rise, hold
 *  and fall -- so a pressed ACT no longer looks like a Play and says how long
 *  it lasts. Where the highlight is too close to the channel to be seen -- a
 *  yellow channel -- it lights in `blobAction` instead, the colour a blob on
 *  the sphere wears while an action has it. Everything else wears its
 *  channel. */
juce::Colour padBaseColour (PadFunction function, bool clipPlaying,
                            bool actionRunning, juce::Colour channel);

juce::Colour padStatusColour (juce::Colour base, Pattern::Status status,
                              Pattern::Status statusLast, int step,
                              bool oneShotRecording);

}
