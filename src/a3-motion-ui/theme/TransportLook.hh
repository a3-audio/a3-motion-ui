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
 *  **Play is always the triangle.** It used to switch to two bars while the
 *  clip ran, so that the glyph carried the state -- and that cannot work,
 *  because there are three states and a glyph with two shapes can only tell
 *  two of them apart. Running, held, and stopped: after Stop the key showed
 *  the same two bars a pause does, and said something untrue. The shape is the
 *  key's *identity*, the ground is its *state* -- which is the division every
 *  deck uses, and the reason a deck's play key has one symbol printed on it
 *  and a lamp behind it.
 *
 *  Nor does anything here change colour: play is green running or not,
 *  because green is which key this is. */
void drawTransportGlyph (juce::Graphics &g, juce::Rectangle<float> area,
                         TransportKey key);

/** What a transport key's ground is doing. */
enum class TransportGround
{
  Dark,    ///< nothing of this key's is happening
  Lit,     ///< it is happening now
  Waiting, ///< pressed, and waiting for the beat to come round -- blinks
};

/** What each key's ground is told about, in one place.
 *
 *  A struct rather than six booleans in a row: they are all the same type, so
 *  one transposed pair at a call site is a light on the wrong key and nothing
 *  that would fail to compile. The same shape FunctionKeyLook already has. */
struct TransportState
{
  bool recording = false;    ///< a take is running or armed on the shown clip
  bool playing = false;      ///< the shown clip is running
  bool scheduled = false;    ///< pressed, and waiting for the beat
  bool actionActive = false; ///< the accent is still moving
  bool stopPressed = false;  ///< a finger is on Stop at this moment
};

/** Whether a transport key's ground lights, and how.
 *
 *  One rule for the global strip and the pads page, and written down rather
 *  than worked out inside a paint method: "is this key lit" is a question two
 *  screens ask and a person asks in the dark, and two answers to it is one
 *  too many.
 *
 *  **The ground is where a key's state lives**, because the glyph cannot hold
 *  it -- see drawTransportGlyph(). So play lights while the clip runs and
 *  blinks while it waits for the beat, and the shape stays a triangle
 *  throughout.
 *
 *  Two things it deliberately does not read:
 *
 *  * **Whether a finger is down on ACT.** The engine puts a clip's settings
 *    back when the accent's envelope has finished falling, not when the hand
 *    lifts -- so `actionActive` is what is still moving, not what is still
 *    held.
 *  * **Whether Stop has anything to stop.** It is a way out, and a way out
 *    that stayed lit afterwards would be claiming to be somewhere. What it
 *    does read is the *press*: Stop acts instantly and unquantised, so it is
 *    the one key that can never blink while it waits -- and without a flash,
 *    the key that always works is the key that never answers. */
TransportGround transportKeyGround (TransportKey key,
                                    TransportState const &state);

/** Three stacked bars: the mark a menu has had since phones grew one, and by
 *  now the one shape people look for when they want the rest of the options.
 *  Settings has no transport meaning, so it is drawn on its own rather than
 *  through the four above. */
void drawMenuGlyph (juce::Graphics &g, juce::Rectangle<float> area);

/** Whether this pad's mark is one of the four transport shapes. Settings is
 *  the one that is not -- it opens a menu, and drawMenuGlyph() draws it. */
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

/** What a colour has to manage against its ground to be read as text.
 *
 *  WCAG's floor for large text. These are short words at a size a booth is
 *  read at, not body copy, so the stricter 4.5:1 would turn away colours that
 *  are perfectly legible here. */
constexpr float minimumInkContrast = 3.f;

/** `ink` where it can be read on `ground`, `fallback` where it cannot.
 *
 *  Channel colours are chosen to tell four channels apart at a glance, not to
 *  be read as letters -- and the ACTION page writes the action's name in the
 *  slot's colour on the bar's own dark ground, where the blue one disappeared.
 *
 *  Stepping aside rather than lightening, for the reason the pads already
 *  learned: a colour pushed far enough to clear a dark ground is no longer the
 *  colour that identified anything. Better to lose the identity in the one
 *  place it cannot be carried than to lose it everywhere by degrees. */
juce::Colour readableInk (juce::Colour ink, juce::Colour ground,
                          juce::Colour fallback);

/** The same rule reached from a pad. Settings has no colour of its own -- it
 *  opens a menu, and a colour that means nothing makes the ones that mean
 *  something harder to read. */
juce::Colour padFunctionColour (PadFunction function);

/** The colour to draw a pad's mark in, on a pad of colour `ground`.

    The function's own colour wherever it can be read (readableInk), and
    otherwise whichever of black and white stands out more -- one of the two
    always reaches 4.58:1, whatever the ground. A running clip turns its Play
    pad the very colour of the triangle, and a skin's channel colours can sit
    as close to red, green or yellow as they like; the mark has to survive
    both. Settings, which stands for no state, is always black or white. */
juce::Colour padGlyphInk (PadFunction function, juce::Colour ground);

}
