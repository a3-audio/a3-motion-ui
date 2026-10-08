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
#include <a3-motion-ui/components/ControllerLayout.hh>
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

// The whole range is about four fingertips of travel (#65), whatever the
// knob's size. It was four of the knob's own heights: on the device a face
// pot is 27 px, so the range was 108 px -- 14 mm, and a 1 mm wobble moved
// the value 7 %. The finger is what travels, so the finger is the measure.
// Relative from where it lands: JUCE's vertical drag, not the absolute
// position under it.
TEST (PotKnob, TheWholeRangeIsSeveralFingertipsLong)
{
  // The device's panel, as JUCE reports it (7.7 px/mm).
  useDisplayForFingertip (195.0, 1.0);

  for (auto const size : { 27, 60, 120 })
    {
      PotKnob knob;
      knob.setBounds (0, 0, size, size);
      EXPECT_EQ (knob.getMouseDragSensitivity (),
                 juce::roundToInt (fingertipsForTheWholeRange
                                   * static_cast<float> (displayFingertip ())))
          << "knob " << size << " px";
    }
  EXPECT_FLOAT_EQ (fingertipsForTheWholeRange, 4.f);
  EXPECT_EQ (PotKnob{}.getSliderStyle (), juce::Slider::RotaryVerticalDrag);

  useDisplayForFingertip (unknownDisplayDpi, 1.0);
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

// A knob that takes no hand -- a band knob during a take -- is drawn the way
// every disabled control is: the same picture at the skin's disabled alpha,
// fainter everywhere it was drawn and nowhere else (2026-10-08).
TEST (PotKnob, ADisabledKnobIsTheSamePictureFainter)
{
  LookAndFeel_A3 lookAndFeel;
  PotKnob knob;
  knob.setLookAndFeel (&lookAndFeel);
  knob.setBounds (0, 0, 60, 80);
  knob.setValue (0.75, juce::dontSendNotification);
  knob.setLabel ("elv");
  knob.setKnobColour (juce::Colours::hotpink);

  auto const painted = [&knob] {
    juce::Image image (juce::Image::ARGB, 60, 80, true);
    juce::Graphics g (image);
    knob.paintEntireComponent (g, false);
    return image;
  };
  auto const lit = painted ();
  knob.setEnabled (false);
  auto const dim = painted ();

  auto drawn = 0;
  auto fainter = 0;
  for (int y = 0; y < lit.getHeight (); ++y)
    for (int x = 0; x < lit.getWidth (); ++x)
      {
        auto const a = lit.getPixelAt (x, y).getAlpha ();
        auto const b = dim.getPixelAt (x, y).getAlpha ();
        if (a == 0)
          {
            EXPECT_EQ (b, 0) << x << "," << y;
            continue;
          }
        ++drawn;
        if (b < a)
          ++fainter;
      }
  ASSERT_GT (drawn, 0);
  EXPECT_GT (fainter, drawn * 9 / 10) << "the disabled knob is not dimmed";

  knob.setLookAndFeel (nullptr);
}

// Two taps are JUCE's double-click, timed by the platform: a knob that has
// someone to ask hands them over and leaves its own value alone, so the owner
// sets the value once and through the same road as a turn -- the mixer's 3D,
// FREQ and Q reset this way.
TEST (PotKnob, TwoTapsAskTheOwnerAndLeaveTheValueAlone)
{
  PotKnob knob;
  knob.setBounds (0, 0, 60, 80);
  knob.setValue (0.8, juce::dontSendNotification);

  auto asked = 0;
  auto changed = 0;
  knob.onDoubleTapped = [&asked] { ++asked; };
  knob.onValueChange = [&changed] { ++changed; };

  auto const source = juce::Desktop::getInstance ().getMainMouseSource ();
  auto const at = juce::Point<float> (30.f, 40.f);
  juce::MouseEvent const event (
      source, at, {}, juce::MouseInputSource::defaultPressure, 0.f, 0.f, 0.f,
      0.f, &knob, &knob, juce::Time::getCurrentTime (), at,
      juce::Time::getCurrentTime (), 2, false);
  knob.mouseDoubleClick (event);

  EXPECT_EQ (asked, 1);
  EXPECT_EQ (changed, 0);
  EXPECT_DOUBLE_EQ (knob.getValue (), 0.8);
}

namespace
{
juce::MouseEvent
eventOn (PotKnob &knob, int clicks, bool dragged)
{
  auto const source = juce::Desktop::getInstance ().getMainMouseSource ();
  auto const at = juce::Point<float> (10.f, 10.f);
  return juce::MouseEvent (source, at, {},
                           juce::MouseInputSource::defaultPressure, 0.f, 0.f,
                           0.f, 0.f, &knob, &knob,
                           juce::Time::getCurrentTime (), at,
                           juce::Time::getCurrentTime (), clicks, dragged);
}
}

// A finger that lands on a knob and lifts without moving has tapped it, and
// the owner is told (#65): a channel face's pots fill the face now, so a tap
// that meant "show this channel" lands on one of them. A drag is not a tap,
// and the second tap of two is the reset's, not another tap.
TEST (PotKnob, ALiftWithoutMovementIsATap)
{
  PotKnob knob;
  knob.setBounds (0, 0, 60, 120);
  auto taps = 0;
  knob.onTapped = [&taps] { ++taps; };

  auto const tap = [&knob] (int clicks, bool dragged) {
    knob.mouseDown (eventOn (knob, clicks, false));
    knob.mouseUp (eventOn (knob, clicks, dragged));
  };

  tap (1, false);
  EXPECT_EQ (taps, 1);

  tap (1, true);
  EXPECT_EQ (taps, 1) << "a drag";

  tap (2, false);
  EXPECT_EQ (taps, 1) << "the second of two taps";
}
