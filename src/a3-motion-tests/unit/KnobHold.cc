/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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
#include <a3-motion-ui/components/KnobHold.hh>

#include <string>

using namespace a3;

TEST (KnobPlaces, EveryKnobStandsUnderItsOwnName)
{
  for (int k = 0; k < numKnobs; ++k)
    {
      auto const knob = static_cast<Knob> (k);
      auto const place = placeOf (knob);
      auto const spec = place.section == elevationSection
                            ? elevationKnobSpec (place.sub)
                            : motionKnobSpec (place.sub);

      EXPECT_EQ (std::string (spec.label), std::string (knobName (knob)));
      ASSERT_TRUE (knobAt (place.section, place.sub).has_value ());
      EXPECT_EQ (*knobAt (place.section, place.sub), knob);
    }
}

TEST (KnobPlaces, FadeAndBiasAreNotRecorded)
{
  // They are how the take is joined, not something it plays.
  EXPECT_FALSE (knobAt (motionSection, 8).has_value ());
  EXPECT_FALSE (knobAt (motionSection, 9).has_value ());
  EXPECT_FALSE (knobAt (0, 1).has_value ());
}

TEST (KnobHold, NothingIsHeldToBeginWith)
{
  KnobHold hold;
  EXPECT_FALSE (hold.isHeld (Knob::Reach, 0.0));
}

TEST (KnobHold, AFingerHoldsUntilItLifts)
{
  KnobHold hold;
  hold.press (Knob::Reach);
  EXPECT_TRUE (hold.isHeld (Knob::Reach, 10000.0))
      << "a finger resting still is still a hand on the knob";

  hold.release (Knob::Reach);
  EXPECT_FALSE (hold.isHeld (Knob::Reach, 10000.0));
}

TEST (KnobHold, AnEncoderStepHoldsForAMoment)
{
  // An encoder has no touch: turning it is the only sign of a hand, so each
  // step holds for a moment and a run of steps holds throughout.
  KnobHold hold;
  hold.nudge (Knob::Spin, 1000.0);

  EXPECT_TRUE (hold.isHeld (Knob::Spin, 1000.0 + KnobHold::encoderHoldMs * 0.5));
  EXPECT_FALSE (hold.isHeld (Knob::Spin, 1000.0 + KnobHold::encoderHoldMs * 1.5));
}

TEST (KnobHold, EachKnobIsHeldOnItsOwn)
{
  KnobHold hold;
  hold.press (Knob::Elevation);

  EXPECT_TRUE (hold.isHeld (Knob::Elevation, 0.0));
  EXPECT_FALSE (hold.isHeld (Knob::Sway, 0.0));
}

TEST (KnobHold, LettingGoOfEverythingEndsEveryHold)
{
  KnobHold hold;
  hold.press (Knob::Elevation);
  hold.nudge (Knob::Spin, 0.0);

  hold.clear ();

  EXPECT_FALSE (hold.isHeld (Knob::Elevation, 0.0));
  EXPECT_FALSE (hold.isHeld (Knob::Spin, 0.0));
}
