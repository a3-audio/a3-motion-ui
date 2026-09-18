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

#include <a3-motion-engine/TakeLaps.hh>

#include <JuceHeader.h>

#include <gtest/gtest.h>

using namespace a3;

// A take is a fixed number of bars, and recording runs on round and round
// inside it. Traced on 2026-09-17: a take of 1024 ticks had been written for
// 6766 -- six and a half laps -- so its first 622 ticks came from the last lap
// and the rest from the one before, with a jump of 1.14 radii where they meet.
// That seam is what reads as a hole. A lap that was not finished is dropped,
// so what plays is one lap, drawn in one go.
TEST (TakeLaps, AnUnfinishedLapIsDropped)
{
  EXPECT_FALSE (takeKeepsPartialLap (1024 + 622, 1024));
  EXPECT_FALSE (takeKeepsPartialLap (6766, 1024));
}

// Unless no lap was ever finished: dropping that would leave nothing at all.
TEST (TakeLaps, TheFirstLapIsKeptHoweverShortItIs)
{
  EXPECT_TRUE (takeKeepsPartialLap (1, 1024));
  EXPECT_TRUE (takeKeepsPartialLap (1023, 1024));
}

TEST (TakeLaps, AWholeNumberOfLapsHasNothingToDrop)
{
  EXPECT_FALSE (takeKeepsPartialLap (1024, 1024));
  EXPECT_FALSE (takeKeepsPartialLap (4096, 1024));
}

TEST (TakeLaps, ATakeWithNoLengthIsLeftAlone)
{
  EXPECT_TRUE (takeKeepsPartialLap (100, 0));
}

// Nothing decides this but the engine, and a rule with no caller decides
// nothing.
TEST (TakeLaps, TheEngineAsksIt)
{
  juce::File const root (A3_ENGINE_SOURCE_DIR);
  auto callers = 0;
  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc", juce::File::findFiles))
    if (entry.getFile ().loadFileAsString ().contains ("takeKeepsPartialLap ("))
      ++callers;

  EXPECT_GT (callers, 0) << "every take keeps its unfinished lap again";
}
