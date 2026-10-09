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

#include <a3-motion-engine/PlaybackRate.hh>
#include <a3-motion-engine/RecMode.hh>
#include <a3-motion-engine/flight/PilotLevel.hh>
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

// Developer mode survives a restart -- asked for on 2026-09-21: maintaining the
// factory clips runs over days, and switching it back on every start was the
// cost the other answer would have had.
TEST (SettingsPersistence, DeveloperModeSurvivesARestart)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-developer-mode-settings.json");
  file.deleteFile ();

  AppSettings settings;
  settings.developerMode = true;
  saveSettings (file, settings);

  EXPECT_TRUE (loadSettings (file).developerMode);
  file.deleteFile ();
}

// Every settings file on a device predates it, and none of them may switch it
// on: off is what the device has always done.
TEST (SettingsPersistence, AFileWithoutDeveloperModeLeavesItOff)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-developer-mode-legacy.json");
  file.replaceWithText ("{\"clockMode\": 1, \"recMode\": \"Touch\"}");

  EXPECT_FALSE (loadSettings (file).developerMode);
  file.deleteFile ();
}

// The four speeds the Shape section's keys carry. A working habit, like the
// rec mode beside them -- so they belong to the device rather than to a set,
// which would change them under the performer at load time.
//
// A fresh device starts on four *paths* (2026-10-08, .claude/notes/
// auditory-motion-research.md B1): on its first take, one bar long, they lap
// in 4 bars, 2, 1 and 2 beats -- 45 to 360 deg/s at 120 BPM, every one under
// the ~900 deg/s past which a room cannot tell which way a sound turns
// (Feron 2010), each a doubling, which is what it takes to be heard as faster
// (Carlile & Best 2002). The old four lapped a one-bar take in 1/2, 1/4 and
// 1/16 of a beat: three effects and one path. They are still a drag away.
TEST (SettingsPersistence, AFreshDeviceStartsOnFourPathsARoomCanFollow)
{
  AppSettings const fresh;
  EXPECT_EQ (fresh.speedButtonLog2,
             (std::array<int, numSpeedButtons>{ 2, 1, 0, -1 }));

  auto const firstTakeBeats = 4.f;
  for (auto const key : fresh.speedButtonLog2)
    {
      auto const lap = playbackLengthBeats (firstTakeBeats, key);
      EXPECT_GE (lap, 2.f) << key;
      EXPECT_LE (lap, 16.f) << key;
    }
}

// The same for a device that has no settings file at all yet.
TEST (SettingsPersistence, AMissingFileStartsOnTheFreshFour)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-speed-keys-missing.json");
  file.deleteFile ();
  EXPECT_EQ (loadSettings (file).speedButtonLog2,
             (std::array<int, numSpeedButtons>{ 2, 1, 0, -1 }));
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

// Which skin the CLEAN key goes back to. A restart in the middle of a set
// that came back up in clean with nowhere to return to would leave the
// performer hunting through the menu for the skin they were in.
TEST (SettingsPersistence, TheSkinBeforeCleanSurvivesARestart)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-skin-before-clean-settings.json");
  file.deleteFile ();

  AppSettings settings;
  settings.skinBeforeClean = "ember";
  saveSettings (file, settings);

  EXPECT_EQ (loadSettings (file).skinBeforeClean, "ember");
  file.deleteFile ();
}

TEST (SettingsPersistence, AFileWithoutASkinBeforeCleanRemembersNone)
{
  auto const file
      = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("a3-skin-before-clean-legacy.json");
  file.replaceWithText ("{\"clockMode\": 1, \"recMode\": \"Touch\"}");

  EXPECT_TRUE (loadSettings (file).skinBeforeClean.isEmpty ());
  file.deleteFile ();
}

}

// Where the room is looked at from, and how close, comes back after a
// restart (2026-09-26): a view set up for a room is set up once.
TEST (SettingsPersistence, TheCameraSurvivesARestart)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-camera.json");
  file.deleteFile ();

  AppSettings original;
  original.cameraPitch = 0.5f;
  original.cameraTurn = 1.2f;
  original.cameraZoom = 1.5f;
  saveSettings (file, original);

  auto const loaded = loadSettings (file);
  EXPECT_FLOAT_EQ (loaded.cameraPitch, 0.5f);
  EXPECT_FLOAT_EQ (loaded.cameraTurn, 1.2f);
  EXPECT_FLOAT_EQ (loaded.cameraZoom, 1.5f);

  file.deleteFile ();
}

// Which of its two things each encoder is on, on MOTION and on REC, comes
// back after a restart (2026-09-27): the marks are where they were left.
TEST (SettingsPersistence, TheEncoderClicksSurviveARestart)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-clicks.json");
  file.deleteFile ();

  AppSettings original;
  original.encoderClicksMotion = 0b10000101;
  original.encoderClicksRecord = 0b1000;
  saveSettings (file, original);

  auto const loaded = loadSettings (file);
  EXPECT_EQ (loaded.encoderClicksMotion, 0b10000101);
  EXPECT_EQ (loaded.encoderClicksRecord, 0b1000);

  file.deleteFile ();
}

// A file from before looks from straight above, unzoomed.
TEST (SettingsPersistence, AFileWithoutACameraLooksFromAbove)
{
  AppSettings const defaults;
  EXPECT_FLOAT_EQ (defaults.cameraPitch, 0.f);
  EXPECT_FLOAT_EQ (defaults.cameraTurn, 0.f);
  EXPECT_FLOAT_EQ (defaults.cameraZoom, 1.f);
}

// A stored view is held to the limits a gesture has: never from below, never
// upside down, never zoomed past the ends -- whatever a hand-edited file or an
// older limit wrote.
TEST (SettingsPersistence, AStoredCameraIsHeldToItsLimits)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-camlimit.json");
  file.deleteFile ();

  AppSettings wild;
  wild.cameraPitch = 2.5f;
  wild.cameraZoom = 10.f;
  saveSettings (file, wild);
  auto const high = loadSettings (file);
  EXPECT_FLOAT_EQ (high.cameraPitch, juce::MathConstants<float>::halfPi);
  EXPECT_FLOAT_EQ (high.cameraZoom, maxCameraZoom);

  wild.cameraPitch = -0.3f;
  wild.cameraZoom = 0.01f;
  saveSettings (file, wild);
  auto const low = loadSettings (file);
  EXPECT_FLOAT_EQ (low.cameraPitch, 0.f);
  EXPECT_FLOAT_EQ (low.cameraZoom, minCameraZoom);

  file.deleteFile ();
}

TEST (SettingsPersistence, TheFpvViewRoundTrips)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-fpvview.json");
  file.deleteFile ();

  AppSettings settings;
  settings.fpvView = true;
  saveSettings (file, settings);

  EXPECT_TRUE (loadSettings (file).fpvView);
  file.deleteFile ();
}

TEST (SettingsPersistence, AFileWithoutTheViewStartsInFull)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-noview.json");
  file.replaceWithText ("{ \"clockMode\": 1 }");

  EXPECT_FALSE (loadSettings (file).fpvView);
  file.deleteFile ();
}

TEST (SettingsPersistence, ThePilotsLevelRoundTrips)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-pilots.json");
  file.deleteFile ();

  AppSettings settings;
  settings.pilotLevel = PilotLevel::Fly;
  saveSettings (file, settings);

  EXPECT_EQ (loadSettings (file).pilotLevel, PilotLevel::Fly);
  file.deleteFile ();
}

TEST (SettingsPersistence, AFileWithoutAKnownPilotsLevelStartsOff)
{
  auto const file = juce::File::getSpecialLocation (
                        juce::File::SpecialLocationType::tempDirectory)
                        .getChildFile ("a3-motion-ui-test-settings-nopilots.json");
  file.replaceWithText ("{ \"clockMode\": 1 }");
  EXPECT_EQ (loadSettings (file).pilotLevel, PilotLevel::Off);
  file.replaceWithText ("{ \"pilotLevel\": \"warp\" }");
  EXPECT_EQ (loadSettings (file).pilotLevel, PilotLevel::Off);
  file.deleteFile ();
}
