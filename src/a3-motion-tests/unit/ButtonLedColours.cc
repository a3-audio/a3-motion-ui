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

}
