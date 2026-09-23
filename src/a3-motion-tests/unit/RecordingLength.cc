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

#include <a3-motion-ui/components/RecordingLength.hh>

using namespace a3;

// A clip on show decides: press REC over an eight-bar figure and the take is
// eight bars. That is the whole idea -- the eight length keys and the page
// they stood on are gone, and this is what replaces them.
TEST (RecordingLength, TheShownClipDecides)
{
  EXPECT_EQ (recordingLengthBeats (32, 0, 4), 32);
  EXPECT_EQ (recordingLengthBeats (4, 3, 4), 4);
  EXPECT_EQ (recordingLengthBeats (1, 5, 4), 1);
}

// An empty slot has no length to offer, so the one it was last given decides.
// A set carries that value, which is why the first take into a fresh slot is
// as long as the last one was rather than as long as a number nobody chose.
TEST (RecordingLength, AnEmptySlotFallsBackOnItsLastLength)
{
  // log2 0 at four beats to the bar is one bar.
  EXPECT_EQ (recordingLengthBeats (0, 0, 4), 4);
  // ... 2 is four bars ...
  EXPECT_EQ (recordingLengthBeats (0, 2, 4), 16);
  // ... and -2 is a quarter of one.
  EXPECT_EQ (recordingLengthBeats (0, -2, 4), 1);
}

// The metrum is not four everywhere.
TEST (RecordingLength, TheFallbackFollowsTheBar)
{
  EXPECT_EQ (recordingLengthBeats (0, 0, 3), 3);
  EXPECT_EQ (recordingLengthBeats (0, 1, 3), 6);
}

// A take of no length would end before the downbeat it started on. Neither a
// nonsensical stored length nor a nonsensical clip may produce one.
TEST (RecordingLength, ItIsNeverNothing)
{
  EXPECT_GE (recordingLengthBeats (0, -12, 4), 1);
  EXPECT_GE (recordingLengthBeats (-5, 0, 4), 1);
  EXPECT_GE (recordingLengthBeats (0, 0, 0), 1);
}
