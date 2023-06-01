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

#include "LookAndFeel.hh"

namespace a3
{

const juce::Colour Colours::background{ 0xff292f36 };

LookAndFeel_A3::LookAndFeel_A3 ()
{
  setColour (juce::Slider::thumbColourId, juce::Colours::lightgrey);

  // setColour (juce::Slider::backgroundColourId, juce::Colours::red);
  // setColour (juce::Slider::trackColourId, juce::Colours::red);
  // setColour (juce::Slider::rotarySliderOutlineColourId, Colours::black);

  setColour (juce::Slider::rotarySliderFillColourId,
             juce::Colours::lightgrey.darker ());
}
}
