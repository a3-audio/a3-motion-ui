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

// A distance, not a time. Sixteenths of a beat said how long a recording spent
// closing its seam; the fade now says how far a gap may be for it to be drawn
// through, which is a property of the movement rather than of the tempo -- and
// means the same thing on a device set up differently.
TEST (PatternSpeedAndFade, APatternKnowsHowFarItsFadeReaches)
{
  Pattern pattern;
  pattern.setFadeReach (0.75f);
  EXPECT_FLOAT_EQ (pattern.getFadeReach (), 0.75f);

  // Held inside the dial: a pot cannot ask for more than the whole sphere.
  pattern.setFadeReach (1.5f);
  EXPECT_FLOAT_EQ (pattern.getFadeReach (), 1.f);
  pattern.setFadeReach (-0.5f);
  EXPECT_FLOAT_EQ (pattern.getFadeReach (), 0.f);
}

// Bipolar like spin and swell, and held to the range the pot can reach.
TEST (PatternSpeedAndFade, APatternKnowsWhereItsBridgesLead)
{
  Pattern pattern;
  pattern.setBridgeBias (-3);
  EXPECT_EQ (pattern.getBridgeBias (), -3);

  pattern.setBridgeBias (9);
  EXPECT_EQ (pattern.getBridgeBias (), 4);
  pattern.setBridgeBias (-9);
  EXPECT_EQ (pattern.getBridgeBias (), -4);
}
