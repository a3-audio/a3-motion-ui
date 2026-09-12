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

#include <array>

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

// The four speeds the Shape section's keys carry. A working habit, like the
// rec mode beside them -- so they belong to the device rather than to a set,
// which would change them under the performer at load time.
TEST (SettingsPersistence, TheSpeedKeysStartWhereTheyAlwaysDid)
{
  AppSettings const fresh;
  EXPECT_EQ (fresh.speedButtonLog2,
             (std::array<int, numSpeedButtons>{ 0, -3, -4, -6 }));
}

TEST (SettingsPersistence, TheSpeedKeysSurviveARestart)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-speed-keys-settings.json");
  file.deleteFile ();

  AppSettings settings;
  settings.speedButtonLog2 = { 2, 0, -1, -7 };
  saveSettings (file, settings);

  EXPECT_EQ (loadSettings (file).speedButtonLog2,
             (std::array<int, numSpeedButtons>{ 2, 0, -1, -7 }));

  file.deleteFile ();
}

// A file written before the keys were assignable has to leave the device
// behaving exactly as it did -- the same courtesy the rec mode was given.
TEST (SettingsPersistence, AFileWithoutSpeedKeysCarriesTheOldFour)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-speed-keys-legacy.json");
  file.replaceWithText ("{\"clockMode\": 1}");

  auto const settings = loadSettings (file);
  EXPECT_EQ (settings.clockMode, 1);
  EXPECT_EQ (settings.speedButtonLog2,
             (std::array<int, numSpeedButtons>{ 0, -3, -4, -6 }));

  file.deleteFile ();
}

// A hand-edited file is the one way a speed outside the range can arrive, and
// a key carrying one would play at a speed the drag cannot bring it back from.
TEST (SettingsPersistence, ASpeedOutsideTheRangeIsBroughtBackIntoIt)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-speed-keys-out-of-range.json");
  file.replaceWithText ("{\"speedButtons\": [99, -99, 0, -3]}");

  auto const settings = loadSettings (file);
  EXPECT_EQ (settings.speedButtonLog2[0], speedLog2Max);
  EXPECT_EQ (settings.speedButtonLog2[1], speedLog2Min);
  EXPECT_EQ (settings.speedButtonLog2[2], 0);
  EXPECT_EQ (settings.speedButtonLog2[3], -3);

  file.deleteFile ();
}

// A file naming fewer keys than the device has says nothing about the rest,
// and the rest keep what they had. Hardware outlives file formats.
TEST (SettingsPersistence, AShortListLeavesTheRemainingKeysAlone)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-speed-keys-short.json");
  file.replaceWithText ("{\"speedButtons\": [1]}");

  auto const settings = loadSettings (file);
  EXPECT_EQ (settings.speedButtonLog2[0], 1);
  EXPECT_EQ (settings.speedButtonLog2[1], -3);
  EXPECT_EQ (settings.speedButtonLog2[3], -6);

  file.deleteFile ();
}

}
