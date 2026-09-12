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

#include <a3-motion-ui/components/RecordingIndicator.hh>

using namespace a3;

// Nothing asked for, nothing running: the indicator is not there at all. It is
// the one mark on the bar that keeps a shape of its own, so it must not stand
// around when there is no take.
TEST (RecordingIndicator, NothingAskedForShowsNothing)
{
  EXPECT_EQ (recordingIndicatorFor (false, false), RecordingIndicator::Off);
}

// Between the key and the downbeat. This is the window the whole thing exists
// for: the hand has pressed REC and now has to know *when* to start moving,
// and until this there was nothing on the bar to count off.
TEST (RecordingIndicator, AskedForButNotYetRunningCountsIn)
{
  EXPECT_EQ (recordingIndicatorFor (true, false),
             RecordingIndicator::CountIn);
}

// Once the take is running the count-in is over, whatever else is pending.
// A blink that carried on under a running take would be the loudest thing on
// the screen saying the least -- and it is the state that writes over
// something that does not come back.
TEST (RecordingIndicator, ARunningTakeEndsTheCountIn)
{
  EXPECT_EQ (recordingIndicatorFor (false, true),
             RecordingIndicator::Running);
  EXPECT_EQ (recordingIndicatorFor (true, true), RecordingIndicator::Running)
      << "a take scheduled behind a running one must not reopen the count-in";
}
