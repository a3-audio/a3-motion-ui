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
class ColourPickerComponent : public juce::Component
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

  void paint (juce::Graphics &g) override;
  void mouseDown (juce::MouseEvent const &event) override;
  void mouseDrag (juce::MouseEvent const &event) override;

private:
  /** Hue along the bottom strip, saturation and lightness in the field. */
  void pickFrom (juce::Point<int> position);
  void setFromHSL (float hue, float saturation, float lightness);

  juce::Rectangle<int> _header;
  juce::Rectangle<int> _field;  //< saturation across, lightness down
  juce::Rectangle<int> _hueBar;
  juce::Rectangle<int> _rows;

  void resized () override;

  juce::Colour _colour{ juce::Colours::transparentBlack };
  juce::String _title;
  int _index = 0;      //< which of H, S, L is browsed
  bool _editing = false;
};

}
