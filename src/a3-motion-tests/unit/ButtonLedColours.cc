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

TEST (ButtonLedColours, AConfiguredButtonGetsItsColour)
{
  auto const config = parse (R"({"record": {"r": 220, "g": 30, "b": 40}})");
  auto const colour = buttonLedColour (config, "record");

  EXPECT_EQ (colour.r, 220);
  EXPECT_EQ (colour.g, 30);
  EXPECT_EQ (colour.b, 40);
}

// A button nobody configured has to stay visible: one that lights wrongly is
// still usable, one that stays dark looks broken.
TEST (ButtonLedColours, AnUnconfiguredButtonStaysWhite)
{
  EXPECT_EQ (buttonLedColour (parse (R"({"tap": {"r": 1, "g": 2, "b": 3}})"),
                              "record"),
             ledColourUnassigned);
}

TEST (ButtonLedColours, NoConfigAtAllStaysWhite)
{
  EXPECT_EQ (buttonLedColour (juce::var{}, "record"), ledColourUnassigned);
}

TEST (ButtonLedColours, APartialEntryFallsBackRatherThanGoingDark)
{
  // Missing a channel would otherwise read as 0 and quietly darken the button.
  EXPECT_EQ (buttonLedColour (parse (R"({"record": {"r": 220, "g": 30}})"),
                              "record"),
             ledColourUnassigned);
}

// The whole point is telling them apart, so the shipped set must not repeat a
// colour.
TEST (ButtonLedColours, ShippedConfigGivesEachButtonItsOwn)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &buttonLeds = parsed["buttonLeds"];

  std::set<juce::uint32> seen;
  for (auto const *name : { "record", "tap", "menu", "shift" })
    {
      auto const colour = buttonLedColour (buttonLeds, name);

      EXPECT_FALSE (colour == ledColourUnassigned)
          << name << " has no colour of its own";
      auto const packed = static_cast<juce::uint32> (
          (colour.r << 16) | (colour.g << 8) | colour.b);
      EXPECT_TRUE (seen.insert (packed).second)
          << name << " repeats a colour another button already uses";
    }
}


// A key that does something should say so while nobody is touching it. The
// LEDs used to light only under a finger, which tells you what you already
// know and nothing about the panel you are looking at.

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

// "idle" sits beside the buttons in the same block, so it must not be mistaken
// for one of them.
TEST (ButtonLedColours, IdleIsNotAButton)
{
  auto const config = parse (R"({"idle": {"r": 1, "g": 2, "b": 3},
                                 "record": {"r": 9, "g": 9, "b": 9}})");

  EXPECT_EQ (buttonLedColour (config, "record").r, 9);
  EXPECT_EQ (buttonLedIdleColour (config).r, 1);
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
