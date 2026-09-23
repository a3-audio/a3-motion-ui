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

#include <functional>

namespace a3

{

/**
 * ColourPickerComponent
 *
 * One colour, picked with a finger or dialled in with the encoder. Three
 * rows of 0..255 were a poor way to say "make this a warmer red".
 *
 * HSL rather than HSB, because lightness is the axis a person reaches for:
 * "the same colour but darker" is one number here and two in HSB. The r/g/b
 * the skin file stores is derived on the way out — the file keeps saying
 * what it always said.
 */
class ColourPickerComponent : public juce::Component,
                              private juce::ChangeListener
{
public:
  ColourPickerComponent ();

  void setColour (juce::Colour colour, juce::String const &title);
  juce::Colour getColour () const { return _colour; }

  /** Turn the encoder: pick a row, or move the armed row's value. */
  void navigate (int delta);
  /** Press it: arm the browsed row, or let it go. */
  void toggleEditing ();

  std::function<void ()> onColourChanged;

  /** Tapped on the done button. The Menu button does the same thing; a
   *  touchscreen wants something to touch. */
  std::function<void ()> onDone;

  void paint (juce::Graphics &g) override;
  void mouseDown (juce::MouseEvent const &event) override;
  void mouseUp (juce::MouseEvent const &event) override;
  void mouseDrag (juce::MouseEvent const &event) override;

private:
  void setFromHSL (float hue, float saturation, float lightness);

  /** JUCE's own picking surface tells us it moved -- late, so this is the
   *  backstop rather than the path a finger takes. See takeColourFromSurface.
   */
  void changeListenerCallback (juce::ChangeBroadcaster *source) override;

  /** Read the surface's colour now, rather than when the message loop gets
   *  round to it.
   *
   *  ColourSelector announces itself through ChangeBroadcaster, whose
   *  sendChangeMessage() is `triggerAsyncUpdate()` -- a message-loop hop that
   *  also *coalesces*, so a drag arrives as a handful of samples one tick
   *  behind the finger. The sphere behind this card is meant to change while
   *  you drag, and a tick behind with the middle of the movement dropped is
   *  what "seeing the change" stops being.
   *
   *  So we listen to the surface's mouse events as well and pull the colour
   *  out on each one. The change message still arrives afterwards and finds
   *  nothing left to do.
   */
  void takeColourFromSurface ();

  /** The colour field and the hue strip are juce::ColourSelector's, with
   *  every other part of it turned off.
   *
   *  Those two are big drag areas and it draws them better than the hand-
   *  rolled pair they replace -- a tested HSV surface with a marker that
   *  stays under the finger. Its RGB sliders and swatches stay off on
   *  purpose: ColourSelector::resized() gives each of them 22 pixels, hard
   *  capped, where a control anybody has to hit in the dark gets
   *  fingertipSize (34). Its children are private, so there is no widening
   *  them, and the rows below are ours for exactly that reason -- they are
   *  sized by the skin and they are what the overlay's side strips drag.
   */
  std::unique_ptr<juce::ColourSelector> _selector;

  juce::Rectangle<int> _header;
  juce::Rectangle<int> _doneButton;
  juce::Rectangle<int> _rows;

  void resized () override;

  juce::Colour _colour{ juce::Colours::transparentBlack };
  juce::String _title;
  int _index = 0;      //< which of H, S, L is browsed
  bool _editing = false;
};

}
