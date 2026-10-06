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

#include <a3-motion-ui/components/FirstSourceOnly.hh>

using namespace a3;

namespace
{
constexpr SourceKey finger{ SourceKey::touch, 0 };
constexpr SourceKey emulatedMouse{ SourceKey::mouse, 0 };
constexpr SourceKey secondFinger{ SourceKey::touch, 1 };
}

// One finger on the rig's touchscreen arrives twice: as touch #0 and, about
// 2 ms later, as X's emulated mouse #0 at the same point (input trace,
// 2026-10-06). A juce::Slider takes every source's mouseDown as a new gesture
// -- it re-anchors its drag start and value -- and every source's drag as a
// step, so the two streams pull the knob back and forth behind the finger
// (#64). A knob follows the source that touched it first, until that one
// lets go.
TEST (FirstSourceOnly, TheSecondSourceOfAGestureIsIgnored)
{
  FirstSourceOnly gesture;

  EXPECT_TRUE (gesture.press (finger));
  EXPECT_FALSE (gesture.press (emulatedMouse)) << "X's copy of the same finger";
  EXPECT_TRUE (gesture.follows (finger));
  EXPECT_FALSE (gesture.follows (emulatedMouse));
  EXPECT_FALSE (gesture.follows (secondFinger));

  EXPECT_FALSE (gesture.release (emulatedMouse)) << "not the gesture's end";
  EXPECT_TRUE (gesture.follows (finger));
  EXPECT_TRUE (gesture.release (finger));
  EXPECT_FALSE (gesture.follows (finger));
}

// Whichever comes first leads: on a desk the mouse is all there is.
TEST (FirstSourceOnly, WhicheverSourceComesFirstLeads)
{
  FirstSourceOnly gesture;

  EXPECT_TRUE (gesture.press (emulatedMouse));
  EXPECT_FALSE (gesture.press (finger));
  EXPECT_TRUE (gesture.release (emulatedMouse));

  EXPECT_TRUE (gesture.press (finger)) << "the next gesture is anyone's";
}

// A release that never arrived must not lock the knob for good: when the
// source it follows is no longer down, the next press takes over.
TEST (FirstSourceOnly, ALostReleaseDoesNotLockTheKnob)
{
  FirstSourceOnly gesture;

  EXPECT_TRUE (gesture.press (finger));
  gesture.forgetIfNotDown ([] (SourceKey) { return false; });
  EXPECT_TRUE (gesture.press (emulatedMouse));

  gesture.forgetIfNotDown ([] (SourceKey) { return true; });
  EXPECT_TRUE (gesture.follows (emulatedMouse)) << "still down, still leads";
}
