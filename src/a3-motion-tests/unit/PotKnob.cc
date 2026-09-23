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

#include <gtest/gtest.h>

#include <a3-motion-ui/components/LookAndFeel.hh>
#include <a3-motion-ui/components/BarKnob.hh>
#include <a3-motion-ui/components/MixerComponent.hh>
#include <a3-motion-ui/components/PotKnob.hh>
#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

// A knob is JUCE's rotary slider, dragged up and down the way every knob on
// this device is, and with no text box: the value is the arc, and the caption
// under it is the knob's own.
TEST (PotKnob, ItIsARotarySliderDraggedVertically)
{
  PotKnob knob;

  EXPECT_EQ (knob.getSliderStyle (), juce::Slider::RotaryVerticalDrag);
  EXPECT_EQ (knob.getTextBoxPosition (), juce::Slider::NoTextBox);
  EXPECT_EQ (knob.getMinimum (), 0.0);
  EXPECT_EQ (knob.getMaximum (), 1.0);
}

// The whole range is a few of the knob's own heights of finger travel -- no
// pixel count of its own, so a bigger pot (the skin's potSize) is a longer
// drag and the feel follows the size on any screen.
TEST (PotKnob, TheDragIsMeasuredInKnobHeights)
{
  PotKnob knob;

  knob.setBounds (0, 0, 60, 80);
  EXPECT_EQ (knob.getMouseDragSensitivity (),
             juce::roundToInt (80 * knobHeightsForTheWholeRange));

  knob.setBounds (0, 0, 40, 40);
  EXPECT_EQ (knob.getMouseDragSensitivity (),
             juce::roundToInt (40 * knobHeightsForTheWholeRange));
}

// What the arc has to say beyond the value: which way it fills, whether it is
// a ring, and where something else is holding it.
TEST (PotKnob, ItCarriesWhatTheArcDraws)
{
  PotKnob knob;

  EXPECT_FALSE (knob.fillsFromTheMiddle ());
  EXPECT_FALSE (knob.wraps ());
  EXPECT_LT (knob.reach (), -1.f);

  // Drawn in its own colour rather than the muted grey a deselected control
  // wears -- which is how every knob on the mixer pages has always looked.
  EXPECT_TRUE (knob.isSelected ());
  EXPECT_FALSE (knob.isActive ());

  knob.setFillsFromTheMiddle (true);
  knob.setWraps (true);
  knob.setReach (0.25f);
  knob.setLabel ("REACH");

  EXPECT_TRUE (knob.fillsFromTheMiddle ());
  EXPECT_TRUE (knob.wraps ());
  EXPECT_FLOAT_EQ (knob.reach (), 0.25f);
  EXPECT_EQ (knob.label (), "REACH");
}

// The picture is the one this device has always drawn: LookAndFeel_A3 hands
// the knob to paintBarKnob, so the arc on a slider and the arc a page draws
// by hand are the same picture, pixel for pixel.
TEST (PotKnob, TheLookAndFeelDrawsItAsTheDevicesOwnKnob)
{
  LookAndFeel_A3 lookAndFeel;
  PotKnob knob;
  knob.setLookAndFeel (&lookAndFeel);
  knob.setBounds (0, 0, 60, 80);
  knob.setValue (0.75, juce::dontSendNotification);
  knob.setLabel ("GAIN");
  knob.setKnobColour (juce::Colours::hotpink);

  juce::Image asSlider (juce::Image::ARGB, 60, 80, true);
  {
    juce::Graphics g (asSlider);
    knob.paintEntireComponent (g, false);
  }

  juce::Image byHand (juce::Image::ARGB, 60, 80, true);
  {
    juce::Graphics g (byHand);
    paintBarKnob (g, knob.getLocalBounds (), mixerControlMetrics (),
                  juce::Colours::hotpink, "GAIN", 0.75f * 2.f - 1.f, false,
                  knob.isActive (), knob.isSelected ());
  }

  auto differing = 0;
  for (int y = 0; y < asSlider.getHeight (); ++y)
    for (int x = 0; x < asSlider.getWidth (); ++x)
      if (asSlider.getPixelAt (x, y) != byHand.getPixelAt (x, y))
        ++differing;

  EXPECT_EQ (differing, 0);

  knob.setLookAndFeel (nullptr);
}
