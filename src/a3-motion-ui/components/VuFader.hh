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

/** The fader a VU meter is: JUCE's slider, drawn as the handle over the meter
 *  the page paints behind it.
 *
 *  A slider rather than our own TouchControl and our own arithmetic, because
 *  the drag is a solved problem and ours was not: the hand-rolled one stepped
 *  in 2 % jumps, then moved one to one but repainted the whole page for every
 *  pixel, and left the handle's outline behind wherever a drag had started.
 *  A slider is its own component, so what it redraws is its own rectangle.
 *
 *  Relative, never absolute: a finger landing low on a playing channel must
 *  not pull it down (setSliderSnapsToMousePosition (false)). Stepless, over
 *  0..1, with no text box -- the meter is behind it and the value is the
 *  handle's position.
 */
class VuFader : public juce::Slider
{
public:
  VuFader ();

  /** The handle's colour: a channel's own, or the skin's text colour for the
   *  master. Set as the slider's own `thumbColourId`, which is where
   *  LookAndFeel_A3::drawLinearSlider reads it -- the drawing is ours, the
   *  way it is coloured is JUCE's. */
  void setHandleColour (juce::Colour colour);

  /** Two taps on this fader. Empty for the master, whose full volume is the
   *  whole room at once. */
  std::function<void ()> onDoubleTapped;

  void mouseDown (juce::MouseEvent const &event) override;
  void mouseDrag (juce::MouseEvent const &event) override;
  void mouseUp (juce::MouseEvent const &event) override;
  void mouseDoubleClick (juce::MouseEvent const &event) override;

private:
  /** Whether this gesture took hold of the cap. A press that missed it is
   *  ignored for as long as the finger is down. */
  bool _grabbed = false;
};

}
