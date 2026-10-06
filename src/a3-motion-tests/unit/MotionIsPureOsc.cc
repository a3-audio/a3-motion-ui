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

namespace
{

// Motion is a pure OSC interface: it sends positions and control values to
// A3 Core and renders no audio (decided 2026-10-06). Core does the audio,
// StemDeck plays. The audio engine Plan 1 had put here behind a CMake switch
// moved to its own repository, a3-audio/a3-engine (parked). These tests hold the rule where it can
// break -- in what the app is built from -- rather than in a comment.
//
// JUCE's Standalone plugin format will not compile without the audio device
// module, so the module stays linked; what is held instead is that no device
// type is compiled into it and no source of ours reaches for one.

juce::File
sourceRoot ()
{
  return juce::File (A3_UI_SOURCE_DIR).getParentDirectory ();
}

juce::File
repositoryRoot ()
{
  return sourceRoot ().getParentDirectory ();
}

// What would open, host or switch on an audio device or the old engine.
juce::StringArray
audioWords ()
{
  return { "AudioDeviceManager",
           "AudioProcessorPlayer",
           "AudioIODevice",
           "JUCE_JACK=1",
           "JUCE_ALSA=1",
           "A3_AUDIO_ENGINE_ENABLED",
           "a3-audio-engine" };
}

juce::Array<juce::File>
appAndEngineSources ()
{
  juce::Array<juce::File> files;
  for (auto const &dir : { juce::File (A3_UI_SOURCE_DIR),
                           juce::File (A3_ENGINE_SOURCE_DIR) })
    for (auto const &pattern : { "*.cc", "*.hh", "*.h", "*.cpp", "*.in",
                                 "CMakeLists.txt" })
      files.addArray (dir.findChildFiles (juce::File::findFiles, true, pattern));
  files.add (repositoryRoot ().getChildFile ("CMakeLists.txt"));
  return files;
}

juce::String
buildReport ()
{
  // Written at generate time from the app target's own properties, so a
  // switch set on the command line or by a later CMake file shows here too.
  return juce::File (A3_UI_BUILD_REPORT).loadFileAsString ();
}

} // namespace

TEST (MotionIsPureOsc, TheSourcesAreWhereTheScanLooks)
{
  // A scan of an empty tree passes for anything.
  EXPECT_GT (appAndEngineSources ().size (), 100);
  EXPECT_TRUE (juce::File (A3_UI_SOURCE_DIR)
                   .getChildFile ("StandaloneApp.cc")
                   .existsAsFile ());
}

TEST (MotionIsPureOsc, NoSourceOpensOrHostsAnAudioDevice)
{
  for (auto const &file : appAndEngineSources ())
    {
      auto const text = file.loadFileAsString ();
      for (auto const &word : audioWords ())
        EXPECT_FALSE (text.contains (word))
            << word << " in " << file.getFullPathName ();
    }
}

TEST (MotionIsPureOsc, ThereIsNoAudioEngineDirectory)
{
  EXPECT_FALSE (sourceRoot ().getChildFile ("a3-audio-engine").exists ());
}

TEST (MotionIsPureOsc, TheReportHoldsTheAppTarget)
{
  // A report that lost the app's properties would pass every check below.
  EXPECT_TRUE (buildReport ().contains ("JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP"))
      << buildReport ();
}

TEST (MotionIsPureOsc, TheAppCompilesInNoAudioDeviceType)
{
  // JUCE_ALSA defaults to 1 on Linux, JUCE_JACK to 0; both are pinned off so
  // the binary has no device type that anything could open.
  auto const report = buildReport ();
  EXPECT_TRUE (report.contains ("JUCE_ALSA=0")) << report;
  EXPECT_TRUE (report.contains ("JUCE_JACK=0")) << report;
}

TEST (MotionIsPureOsc, TheAppTargetLinksAndSwitchesOnNoAudioEngine)
{
  auto const report = buildReport ();
  for (auto const &word : audioWords ())
    EXPECT_FALSE (report.contains (word)) << word << " in\n" << report;
}
