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

// The Motion row pairs a standing value with the sweep that works on it: the
// values are continuous, the sweeps count whole LFO steps either way.
TEST (ClipKnobs, TheMotionRowPairsValuesWithSweeps)
{
  for (auto const sweep : { 1, 3, 5, 7 })
    {
      EXPECT_EQ (motionKnobSpec (sweep).min, -lfoMaxStep) << sweep;
      EXPECT_EQ (motionKnobSpec (sweep).max, lfoMaxStep) << sweep;
      EXPECT_EQ (motionKnobSpec (sweep).interval, 1.0) << sweep;
      EXPECT_TRUE (motionKnobSpec (sweep).bipolar) << sweep;
    }

  for (auto const value : { 2, 4, 6 })
    {
      EXPECT_EQ (motionKnobSpec (value).min, -1.0) << value;
      EXPECT_EQ (motionKnobSpec (value).max, 1.0) << value;
      EXPECT_EQ (motionKnobSpec (value).interval, 0.0) << value;
      EXPECT_TRUE (motionKnobSpec (value).bipolar) << value;
    }
}

// A turn comes round to itself, so its scale is a ring rather than a stop --
// the same thing Pattern::setRotate does with the value.
TEST (ClipKnobs, TheRotationIsARing)
{
  EXPECT_TRUE (motionKnobSpec (0).wraps);
  EXPECT_EQ (motionKnobSpec (0).min, 0.0);
  EXPECT_EQ (motionKnobSpec (0).max, 1.0);

  for (auto const other : { 1, 2, 8, 9 })
    EXPECT_FALSE (motionKnobSpec (other).wraps) << other;
}

// The bridge's bias leans in whole notches, four either way, as the engine
// holds it.
TEST (ClipKnobs, TheBiasLeansInWholeNotches)
{
  EXPECT_EQ (motionKnobSpec (9).min, -4.0);
  EXPECT_EQ (motionKnobSpec (9).max, 4.0);
  EXPECT_EQ (motionKnobSpec (9).interval, 1.0);
}

TEST (ClipKnobs, NothingMovingAKnobDrawsNoArc)
{
  EXPECT_EQ (reachOnKnob (motionKnobSpec (0), std::nullopt), -2.f);
  EXPECT_EQ (reachOnKnob (motionKnobSpec (2), std::nullopt), -2.f);
}

TEST (ClipKnobs, TheSpinsHoldOnTheRotationIsARingAngle)
{
  // rot runs 0..1 round a ring, which PotKnob draws as 0..2.
  EXPECT_FLOAT_EQ (reachOnKnob (motionKnobSpec (0), 0.25f), 0.5f);
  EXPECT_FLOAT_EQ (reachOnKnob (motionKnobSpec (0), 0.f), 0.f);
}

TEST (ClipKnobs, TheSwellAndTheStretchesHoldOnTheirOwnScale)
{
  // reach and the two squeezes run -1..1, which is already the angle.
  for (auto const sub : { 2, 4, 6 })
    {
      EXPECT_FLOAT_EQ (reachOnKnob (motionKnobSpec (sub), 0.3f), 0.3f) << sub;
      EXPECT_FLOAT_EQ (reachOnKnob (motionKnobSpec (sub), -1.f), -1.f) << sub;
      EXPECT_FLOAT_EQ (reachOnKnob (motionKnobSpec (sub), 1.f), 1.f) << sub;
    }
}
