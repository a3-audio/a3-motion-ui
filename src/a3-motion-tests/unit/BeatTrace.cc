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

#include <a3-motion-engine/tempo/BeatTrace.hh>

#include <JuceHeader.h>

#include <gtest/gtest.h>

namespace a3
{

namespace
{
juce::File
freshTraceFile ()
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("beat-trace", ".csv");
  return file;
}
}

TEST (BeatTrace, WithoutAPathItWritesNothing)
{
  // Off by default: a device playing a set must not grow a file.
  BeatTrace trace ("");
  EXPECT_FALSE (trace.isEnabled ());
  trace.record ("rx", 1, 2, 120.5f);
}

TEST (BeatTrace, EachEventIsOneLineOnTheMonotonicClock)
{
  auto const file = freshTraceFile ();
  {
    BeatTrace trace (file.getFullPathName ().toStdString ());
    ASSERT_TRUE (trace.isEnabled ());
    trace.record ("rx", 3, 17, 124.5f);
    trace.record ("engine", 0, 18, 124.0f);
  }

  juce::StringArray lines;
  file.readLines (lines);
  lines.removeEmptyStrings ();
  file.deleteFile ();

  // A header naming the columns and how to turn the clock into wall time,
  // which is what the beat-analyzer's journal is stamped in.
  ASSERT_EQ (lines.size (), 3);
  EXPECT_TRUE (lines[0].startsWith ("# monotonic_us,event,beat,bar,bpm"));
  EXPECT_TRUE (lines[0].contains ("realtime_minus_monotonic_us="));

  auto const first = juce::StringArray::fromTokens (lines[1], ",", "");
  ASSERT_EQ (first.size (), 5);
  EXPECT_GT (first[0].getLargeIntValue (), 0);
  EXPECT_EQ (first[1], "rx");
  EXPECT_EQ (first[2], "3");
  EXPECT_EQ (first[3], "17");
  // The tempo as it arrived, not rounded: the rounding is one of the things
  // being looked for.
  EXPECT_EQ (first[4], "124.500");

  auto const second = juce::StringArray::fromTokens (lines[2], ",", "");
  EXPECT_GE (second[0].getLargeIntValue (), first[0].getLargeIntValue ());
  EXPECT_EQ (second[1], "engine");
}

}
