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

#include <a3-motion-ui/components/ActionKnobs.hh>

using namespace a3;

// The two stages of a row count whole steps, as the engine holds them; the
// ceiling is continuous.
TEST (ActionKnobs, TheStagesAreWholeStepsAndTheCeilingIsNot)
{
  for (auto const stage : { 0, 1, 3, 4, 6, 7 })
    {
      EXPECT_FALSE (actionKnobIsACeiling (stage)) << stage;
      EXPECT_EQ (actionKnobSpec (stage).max, envelopeMaxStep) << stage;
      EXPECT_EQ (actionKnobSpec (stage).interval, 1.0) << stage;
    }

  for (auto const ceiling : { 2, 5, 8 })
    {
      EXPECT_TRUE (actionKnobIsACeiling (ceiling)) << ceiling;
      EXPECT_EQ (actionKnobSpec (ceiling).max, 1.0) << ceiling;
      EXPECT_EQ (actionKnobSpec (ceiling).interval, 0.0) << ceiling;
    }
}

// Two taps put a knob back: a stage half way, the accent's ceiling half, a
// filter's off.
TEST (ActionKnobs, TwoTapsPutAKnobBack)
{
  EXPECT_EQ (actionKnobSpec (0).resetTo, envelopeMaxStep / 2);
  EXPECT_EQ (actionKnobSpec (2).resetTo, 0.5);
  EXPECT_EQ (actionKnobSpec (5).resetTo, 0.0);
  EXPECT_EQ (actionKnobSpec (8).resetTo, 0.0);
}
