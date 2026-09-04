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

#include <a3-motion-ui/components/ClipSettingsLayout.hh>
#include <a3-motion-ui/io/PadFunctions.hh>

namespace a3
{

/** What each of the four clip actions is written in, wherever it is written.
 *
 *  The words REC, STOP, PLAY and ACT appear in the bar's header, on the pads
 *  page, on the global strip and beside the accent's depth. They were coloured
 *  where each of them happened to be drawn, which is how the same word ended
 *  up meaning one thing in one corner of the screen and nothing in another.
 *  One rule, read by every one of those places.
 *
 *  Red for the two that take something away -- recording writes over what you
 *  cannot get back, stop ends what is running. Green for a clip that is
 *  actually playing and red for one that is not, because that is the single
 *  question you ask a transport at a glance. Yellow for the accent: not a
 *  state you are in but a thing you do while you hold it.
 */
juce::Colour transportColour (TransportKey key);

/** And what each of them is drawn as, wherever it is drawn.
 *
 *  A circle records, a square stops, a triangle plays and two bars hold. These
 *  are the shapes every deck and every transport has used for fifty years, and
 *  they are read without being read -- which is the whole reason for using
 *  them on the four controls you reach for while looking somewhere else. ACT
 *  has no such shape and gets its initial instead; a symbol invented for it
 *  would have to be learned, and a shape that has to be learned is a word.
 *
 *  `playing` chooses between the triangle and the bars: what a transport key
 *  shows is what pressing it will do. It does not change any colour -- play is
 *  green whether or not it is running, because green is which key this is. */
void drawTransportGlyph (juce::Graphics &g, juce::Rectangle<float> area,
                         TransportKey key, bool playing);

/** The glyph for a pad, or nothing for Settings, which opens a menu and has no
 *  shape that would mean that. */
bool hasTransportGlyph (PadFunction function);
TransportKey transportKeyForPad (PadFunction function);

/** Relative luminance and contrast ratio, per WCAG 2.1.
 *
 *  Here rather than only in the skin script because these colours land on
 *  grounds the skin does not choose -- a pad carries its channel's colour, so
 *  ACT's yellow lands on a yellow pad and STOP's red on a red one. Bending the
 *  ink until it read was tried first and does not work: pure red on channel
 *  one's pink tops out at 2.1:1 however it is lightened or darkened, and a red
 *  that has been pushed far enough to clear is no longer red. So the word gets
 *  its own dark ground instead and keeps its colour exactly, and these are
 *  what the tests hold that ground to. */
float relativeLuminance (juce::Colour colour);
float contrastRatio (juce::Colour a, juce::Colour b);

/** The same rule reached from a pad. Settings has no colour of its own -- it
 *  opens a menu, and a colour that means nothing makes the ones that mean
 *  something harder to read. */
juce::Colour padFunctionColour (PadFunction function);

}
