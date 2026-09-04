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

#include <a3-motion-engine/Pattern.hh>

using namespace a3;

// Speed and fade belong to the clip, not to the screen that shows it: the
// engine reads them and they have to survive being saved. They lived in the
// UI's own per-slot table, where a file could not reach them -- which is why
// a clip's speed did not come back after a restart.
TEST (PatternSpeedAndFade, APatternKnowsHowFastItPlays)
{
  Pattern pattern;
  EXPECT_EQ (pattern.getSpeedLog2 (), 0) << "one bar is the default";

  pattern.setSpeedLog2 (-2);
  EXPECT_EQ (pattern.getSpeedLog2 (), -2);
}

// In sixteenths of a beat, as the panel offers it -- not in ticks. Ticks come
// out of the PPQN the take was written with, so a value in ticks means
// something else on a device set up differently.
TEST (PatternSpeedAndFade, APatternKnowsHowItsLoopCloses)
{
  Pattern pattern;
  pattern.setFadeSixteenths (12);
  EXPECT_EQ (pattern.getFadeSixteenths (), 12);
}
