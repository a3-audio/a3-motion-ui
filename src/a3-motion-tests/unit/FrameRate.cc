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

#include <a3-motion-ui/FrameRate.hh>

#include <gtest/gtest.h>

using namespace a3;

TEST (FrameRateTrace, SaysNothingBeforeTheWindowHasGoneBy)
{
  FrameRate rate{ 2.0 };

  EXPECT_TRUE (rate.tick (100.0).isEmpty ());
  EXPECT_TRUE (rate.tick (100.5).isEmpty ());
  EXPECT_TRUE (rate.tick (101.9).isEmpty ());
}

TEST (FrameRateTrace, ReportsFramesOverTheElapsedTimeNotOverTheWindow)
{
  FrameRate rate{ 2.0 };

  // Twenty a second, and the frame that starts the window is not one of
  // them -- counted, it would be the app's whole startup divided into one.
  rate.tick (0.0);

  juce::String line;
  for (auto i = 1; i <= 41; ++i)
    {
      auto const reported = rate.tick (static_cast<double> (i) * 0.05);
      if (reported.isNotEmpty ())
        line = reported;
    }

  EXPECT_TRUE (line.contains ("20.0")) << line;
}

// The first frame starts the window rather than ending one -- taken as a
// report it would divide by however long the app took to come up.
TEST (FrameRateTrace, TheFirstFrameNeverReports)
{
  FrameRate rate{ 2.0 };
  EXPECT_TRUE (rate.tick (9999.0).isEmpty ());
}

TEST (FrameRateTrace, StartsCountingAgainAfterEachReport)
{
  FrameRate rate{ 1.0 };

  rate.tick (0.0);
  rate.tick (0.5);
  ASSERT_FALSE (rate.tick (1.5).isEmpty ());

  EXPECT_TRUE (rate.tick (2.0).isEmpty ());
  EXPECT_FALSE (rate.tick (2.6).isEmpty ());
}

// Time that does not move is not a rate, and dividing by it is an infinity in
// the log.
TEST (FrameRateTrace, AStoppedClockReportsNothing)
{
  FrameRate rate{ 0.0 };

  rate.tick (5.0);
  EXPECT_TRUE (rate.tick (5.0).isEmpty ());
}
