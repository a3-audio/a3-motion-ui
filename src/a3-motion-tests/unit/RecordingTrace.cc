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

#include <a3-motion-engine/RecordingTrace.hh>

#include <JuceHeader.h>

#include <gtest/gtest.h>

namespace a3
{

TEST (RecordingTrace, WithoutAPathItWritesNothing)
{
  RecordingTrace trace ("");
  EXPECT_FALSE (trace.isEnabled ());
  trace.wrote (17, 0.5f, -0.25f, true, 17);
}

TEST (RecordingTrace, EveryWrittenTickIsOneLine)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("rec-trace", ".csv");
  {
    RecordingTrace trace (file.getFullPathName ().toStdString ());
    ASSERT_TRUE (trace.isEnabled ());
    trace.wrote (17, 0.5f, -0.25f, true, 17);
    trace.wrote (18, 0.5f, -0.25f, false, 18);
  }

  juce::StringArray lines;
  file.readLines (lines);
  lines.removeEmptyStrings ();
  file.deleteFile ();

  ASSERT_EQ (lines.size (), 3);
  EXPECT_TRUE (lines[0].startsWith ("# tick,x,y,fingerDown,ticksSinceStart"));

  auto const first = juce::StringArray::fromTokens (lines[1], ",", "");
  ASSERT_EQ (first.size (), 5);
  EXPECT_EQ (first[0], "17");
  EXPECT_EQ (first[1], "0.5000");
  EXPECT_EQ (first[2], "-0.2500");
  EXPECT_EQ (first[3], "1");
  EXPECT_EQ (first[4], "17");
  EXPECT_EQ (juce::StringArray::fromTokens (lines[2], ",", "")[3], "0");
}

// What the take holds when it is finished -- the array the file is written
// from, with the holes still in it.
TEST (RecordingTrace, TheFinishedTakeIsWrittenOutTickByTick)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("rec-trace", ".csv");
  {
    RecordingTrace trace (file.getFullPathName ().toStdString ());
    trace.finished ("Rec_01", { Pos::fromCartesian (0.25f, 0.5f, 0.f),
                                Pos::invalid });
  }

  juce::StringArray lines;
  file.readLines (lines);
  lines.removeEmptyStrings ();
  file.deleteFile ();

  ASSERT_EQ (lines.size (), 4);
  EXPECT_TRUE (lines[1].startsWith ("# finished Rec_01 ticks=2"));
  EXPECT_EQ (lines[2], "final,0,0.2500,0.5000,1");
  EXPECT_EQ (lines[3], "final,1,0.0000,0.0000,0") << "an unwritten tick";
}

// A trace nothing feeds is a trace that proves nothing.
TEST (RecordingTrace, TheEngineActuallyFeedsIt)
{
  juce::File const root (A3_ENGINE_SOURCE_DIR);
  auto callers = 0;
  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc", juce::File::findFiles))
    if (entry.getFile ().getFileName () != "RecordingTrace.cc"
        && entry.getFile ().loadFileAsString ().contains ("RecordingTrace::device ()"))
      ++callers;

  EXPECT_GT (callers, 0) << "nothing writes what a take receives";
}

}
