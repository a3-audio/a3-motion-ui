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

#include <a3-motion-ui/components/ClipKnobs.hh>

using namespace a3;

// The two clips are continuous shares of the height; the sway counts whole
// LFO steps either way, so its arc grows from the middle.
TEST (ClipKnobs, TheElevationSectionsScales)
{
  for (auto const clip : { 0, 1 })
    {
      EXPECT_EQ (elevationKnobSpec (clip).min, 0.0) << clip;
      EXPECT_EQ (elevationKnobSpec (clip).max, 1.0) << clip;
      EXPECT_EQ (elevationKnobSpec (clip).interval, 0.0) << clip;
      EXPECT_FALSE (elevationKnobSpec (clip).bipolar) << clip;
    }

  EXPECT_EQ (elevationKnobSpec (2).min, -lfoMaxStep);
  EXPECT_EQ (elevationKnobSpec (2).max, lfoMaxStep);
  EXPECT_EQ (elevationKnobSpec (2).interval, 1.0);
  EXPECT_TRUE (elevationKnobSpec (2).bipolar);
}

// Two taps put all three back to nothing: no clipping, no sway.
TEST (ClipKnobs, TwoTapsPutTheElevationKnobsBack)
{
  for (auto const sub : { 0, 1, 2 })
    EXPECT_EQ (elevationKnobSpec (sub).resetTo, 0.0) << sub;
}

// The caption belongs to the knob, not to the painting: one place says what a
// control is, and the page only wires it up.
TEST (ClipKnobs, EachKnobCarriesItsCaption)
{
  EXPECT_STREQ (elevationKnobSpec (0).label, caption::clipBottom);
  EXPECT_STREQ (elevationKnobSpec (1).label, caption::clipTop);
  EXPECT_STREQ (elevationKnobSpec (2).label, caption::sway);
}
