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

namespace a3
{

/** A knob: JUCE's rotary slider, drawn as the arc this device has always
 *  drawn (paintBarKnob, through LookAndFeel_A3::drawRotarySlider).
 *
 *  A slider rather than a hit area plus our own arithmetic, for the reasons
 *  the faders were changed over: the drag is JUCE's, the component redraws
 *  only itself, and a value it owns cannot drift from the picture.
 *
 *  What the arc says beyond the value -- which way it fills, whether it is a
 *  closed ring, where something else is holding it, what is written under it
 *  -- lives here, because a LookAndFeel is handed a slider and nothing else.
 */
/** How much finger travel the whole range takes, in the knob's own heights.
 *
 *  Four of them: enough that a value can be set finely, short enough that a
 *  hand can cross the range in one go. Relative to the knob rather than to a
 *  number of pixels, so the skin's pot size decides the feel along with the
 *  look. */
constexpr float knobHeightsForTheWholeRange = 4.f;

class PotKnob : public juce::Slider
{
public:
  PotKnob ();

  void setLabel (juce::String const &label);
  juce::String const &label () const { return _label; }

  /** Bipolar: the arc grows from the middle rather than from the start. */
  void setFillsFromTheMiddle (bool fills);
  bool fillsFromTheMiddle () const { return _fillsFromTheMiddle; }

  /** A closed ring, for a value that comes round to itself. */
  void setWraps (bool wraps);
  bool wraps () const { return _wraps; }

  /** Where something else is holding the value right now -- the spin under
   *  the rotation. Below -1 for nothing. */
  void setReach (float reach);
  float reach () const { return _reach; }

  /** The channel's colour, or the skin's text colour where a control belongs
   *  to nobody in particular. */
  void setKnobColour (juce::Colour colour);

  /** Lit, and picked out of its row. Both are drawn, neither is dragged. */
  void setActive (bool active);
  bool isActive () const { return _active; }
  void setSelected (bool selected);
  bool isSelected () const { return _selected; }

  void resized () override;

private:
  void refreshSensitivity ();

  juce::String _label;
  float _reach = -2.f;
  bool _fillsFromTheMiddle = false;
  bool _wraps = false;
  /** As the mixer's knobs have always been drawn: the colour is the
   *  channel's (selected), and the lit look belongs to the bar's browsing,
   *  which a mixer page has none of. */
  bool _active = false;
  bool _selected = true;
};

}
