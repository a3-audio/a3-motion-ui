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

#include <a3-motion-engine/RecordingName.hh>

#include <set>

using namespace a3;

namespace
{
/** A predicate over a fixed set, which is what the library's name lookup is
 *  from this function's point of view. */
auto
takenBy (std::set<juce::String> const &names)
{
  return [&names] (juce::String const &name) { return names.count (name) > 0; };
}
}

// The day, not the second. A take is found again by when it was made, and
// "the second one from the 8th" is something somebody can say out loud;
// 144918 is not.
TEST (RecordingName, ATakeIsNamedAfterTheDayItWasMade)
{
  auto const when = juce::Time (2026, 8, 8, 14, 49, 18);  // months are 0-based
  EXPECT_EQ (recordingBaseName (when), "Rec_260908");
}

// The number is always there, including on the first take of the day. A bare
// name followed by a counted one reads as two different kinds of thing, and
// the first take of a day is not a different kind of thing.
TEST (RecordingName, TheFirstTakeOfTheDayIsAlreadyNumbered)
{
  EXPECT_EQ (freeRecordingName ("Rec_260908", takenBy ({})), "Rec_260908_01");
}

// A day is not unique the way a second is, so the counter is not decoration:
// two takes on one day would otherwise carry one name, and a set names its
// takes rather than pointing at a file.
TEST (RecordingName, TheNextTakeCountsOn)
{
  EXPECT_EQ (freeRecordingName ("Rec_260908", takenBy ({ "Rec_260908_01" })),
             "Rec_260908_02");
}

TEST (RecordingName, CountingWalksPastEveryNameAlreadyThere)
{
  EXPECT_EQ (freeRecordingName ("Rec_260908",
                                takenBy ({ "Rec_260908_01", "Rec_260908_02",
                                           "Rec_260908_03" })),
             "Rec_260908_04");
}

// Two digits, so ten sorts after nine rather than after one. The library is
// listed by the name that is shown.
TEST (RecordingName, TheCountIsPaddedSoTheListSorts)
{
  std::set<juce::String> nine;
  for (int n = 1; n <= 9; ++n)
    nine.insert ("Rec_260908_0" + juce::String (n));

  EXPECT_EQ (freeRecordingName ("Rec_260908", takenBy (nine)),
             "Rec_260908_10");
}

// Yesterday's takes are not in this day's way.
TEST (RecordingName, AnotherDaysTakesDoNotCount)
{
  EXPECT_EQ (freeRecordingName ("Rec_260908",
                                takenBy ({ "Rec_260907_01", "Rec_260907_02" })),
             "Rec_260908_01");
}
