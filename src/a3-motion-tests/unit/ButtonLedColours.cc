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

#include <JuceHeader.h>

#include <a3-motion-ui/io/ButtonLedColours.hh>

#include <set>

using namespace a3;

namespace
{

juce::var
parse (juce::String const &json)
{
  return juce::JSON::parse (json);
}







// A key that does something should say so while nobody is touching it. The
// LEDs used to light only under a finger, which tells you what you already
// know and nothing about the panel you are looking at.

// `idle` is the only entry of the buttonLeds block anybody reads now, so the
// three tests below are what is left of the per-key ones: missing, half
// written, and set. The lookup they exercised is still there, file-local,
// and this is the one door left into it.
TEST (ButtonLedColours, TheIdleColourIsWhiteUnlessTheConfigSaysOtherwise)
{
  EXPECT_EQ (buttonLedIdleColour (juce::var{}), ledColourUnassigned);
  EXPECT_EQ (buttonLedIdleColour (parse (R"({"record": {"r": 1}})")),
             ledColourUnassigned);
}

TEST (ButtonLedColours, TheIdleColourCanBeSetInTheConfig)
{
  auto const idle = buttonLedIdleColour (
      parse (R"({"idle": {"r": 40, "g": 40, "b": 60}})"));

  EXPECT_EQ (idle.r, 40);
  EXPECT_EQ (idle.g, 40);
  EXPECT_EQ (idle.b, 60);
}

// Same rule as a button's own colour: all three channels or none, because a
// missing one would read as 0 and dim the panel for no stated reason.
TEST (ButtonLedColours, AHalfWrittenIdleColourFallsBackToWhite)
{
  EXPECT_EQ (buttonLedIdleColour (parse (R"({"idle": {"r": 40, "g": 40}})")),
             ledColourUnassigned);
}


}


// The maintainer's report, as a test: the accent green looked white on the
// panel. It is rgb(144, 238, 144) -- a pastel, which a screen reads as green
// because the bar around it is dark, and an LED cannot, because an LED has no
// around.
TEST (ButtonLedColours, APastelIsSaturatedBeforeItReachesAnLed)
{
  auto const pastel = juce::Colour (144, 238, 144);
  auto const lit = ledColour (pastel);

  // Within a hair of the floor rather than at or above it: a colour is three
  // bytes, and a saturation of exactly 0.8 is not one of the values three
  // bytes can hold.
  EXPECT_GE (lit.getSaturation (), ledMinSaturation - 0.01f);
  EXPECT_NEAR (lit.getHue (), pastel.getHue (), 0.01f)
      << "the hue is what the key means";
  EXPECT_NEAR (lit.getBrightness (), pastel.getBrightness (), 0.01f)
      << "how bright a key is already means something else";
}

// A colour that is already a colour is left alone. Pushing everything to full
// would flatten the difference between a warning and a danger.
TEST (ButtonLedColours, AnAlreadySaturatedColourIsUnchanged)
{
  for (auto const colour : { juce::Colour (255, 0, 0),
                             juce::Colour (0, 200, 148),
                             juce::Colour (230, 159, 0) })
    EXPECT_EQ (ledColour (colour), colour);
}

// White is white. A key meant to be white -- carbon's accent, say -- must not
// be given a hue it never had, and a grey has no hue to keep.
TEST (ButtonLedColours, SomethingWithNoHueStaysAsItIs)
{
  for (auto const colour : { juce::Colours::white, juce::Colours::black,
                             juce::Colour (128, 128, 128) })
    EXPECT_EQ (ledColour (colour), colour);
}
