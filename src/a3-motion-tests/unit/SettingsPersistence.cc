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

#include <a3-motion-engine/RecMode.hh>
#include <a3-motion-ui/SettingsPersistence.hh>

using namespace a3;

namespace
{

TEST (SettingsPersistence, MissingFileReturnsDefaults)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-missing.json");
  file.deleteFile ();

  auto const settings = loadSettings (file);
  EXPECT_EQ (settings.clockMode, 0);
}

TEST (SettingsPersistence, MalformedJsonReturnsDefaults)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-malformed.json");
  file.replaceWithText ("{ not valid json");

  auto const settings = loadSettings (file);
  EXPECT_EQ (settings.clockMode, 0);

  file.deleteFile ();
}

TEST (SettingsPersistence, RoundTripsThroughSaveAndLoad)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-roundtrip.json");
  file.deleteFile ();

  AppSettings const original{ 2 };
  saveSettings (file, original);

  auto const loaded = loadSettings (file);
  EXPECT_EQ (loaded.clockMode, original.clockMode);

  file.deleteFile ();
}


// The automation mode is a device setting, not an appearance one, so it lives
// here beside clockMode rather than in the skin.
TEST (SettingsPersistence, TheAutomationModeSurvivesARestart)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-automation-settings.json");
  file.deleteFile ();

  AppSettings settings;
  settings.recMode = RecMode::Write;
  saveSettings (file, settings);

  EXPECT_EQ (loadSettings (file).recMode, RecMode::Write);
  file.deleteFile ();
}

// A file written before this setting existed must not change how the device
// records. Touch is what it has always done.
TEST (SettingsPersistence, AFileWithoutOneRecordsAsBefore)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-automation-legacy.json");
  file.replaceWithText ("{\"clockMode\": 2}");

  auto const settings = loadSettings (file);
  EXPECT_EQ (settings.clockMode, 2);
  EXPECT_EQ (settings.recMode, RecMode::Touch);

  file.deleteFile ();
}

}
