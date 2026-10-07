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

#include <a3-motion-ui/components/CameraFingers.hh>

using namespace a3;

namespace
{
constexpr SourceKey finger{ SourceKey::touch, 0 };
constexpr SourceKey emulatedMouse{ SourceKey::mouse, 0 };
constexpr SourceKey secondFinger{ SourceKey::touch, 1 };
constexpr SourceKey penTip{ SourceKey::pen, 0 };

juce::Point<float> const landing{ 100.f, 120.f };
}

// On the rig one finger arrives twice: touch #0 and, ~2 ms later, X's
// emulated mouse #0 at the same point. Counted as two fingers, the camera
// took them for a pinch and stopped turning (2026-10-07). One finger on the
// glass is one camera finger.
TEST (CameraFingers, AFingersEmulatedMouseIsNotASecondFinger)
{
  CameraFingers fingers;

  EXPECT_TRUE (fingers.press (finger, landing));
  EXPECT_FALSE (fingers.press (emulatedMouse, landing))
      << "X's copy of the same finger";
  EXPECT_EQ (fingers.count (), 1u);

  EXPECT_TRUE (fingers.move (finger, landing + juce::Point<float> (30.f, 0.f)));
  EXPECT_FALSE (fingers.move (emulatedMouse, landing));
  EXPECT_EQ (fingers.count (), 1u);

  EXPECT_FALSE (fingers.release (emulatedMouse)) << "not a camera finger";
  EXPECT_EQ (fingers.count (), 1u);
  EXPECT_TRUE (fingers.release (finger));
  EXPECT_EQ (fingers.count (), 0u);
}

// Two real fingers are touch #0 and touch #1: a pinch, emulated mouse or not.
TEST (CameraFingers, TwoTouchesAreAPinch)
{
  CameraFingers fingers;

  EXPECT_TRUE (fingers.press (finger, { 0.f, 0.f }));
  EXPECT_FALSE (fingers.press (emulatedMouse, { 0.f, 0.f }));
  EXPECT_TRUE (fingers.press (secondFinger, { 30.f, 40.f }));
  EXPECT_EQ (fingers.count (), 2u);
  EXPECT_FLOAT_EQ (fingers.pinchDistance (), 50.f);

  EXPECT_TRUE (fingers.move (secondFinger, { 60.f, 80.f }));
  EXPECT_FLOAT_EQ (fingers.pinchDistance (), 100.f);
}

// On a desk or over VNC the mouse is all there is, and it turns the view; a
// stray touch meanwhile does not turn its drag into a pinch.
TEST (CameraFingers, AMouseAloneLeadsAndAStrayTouchIsIgnored)
{
  CameraFingers fingers;

  EXPECT_TRUE (fingers.press (emulatedMouse, landing));
  EXPECT_FALSE (fingers.press (finger, landing));
  EXPECT_FALSE (fingers.press (penTip, landing));
  EXPECT_EQ (fingers.count (), 1u);
  EXPECT_TRUE (fingers.move (emulatedMouse, landing));
  EXPECT_TRUE (fingers.release (emulatedMouse));

  EXPECT_TRUE (fingers.press (finger, landing)) << "the next gesture is anyone's";
}

// The kind that leads holds the sphere until its last finger is up, not just
// the first one.
TEST (CameraFingers, TheLeadingKindHoldsUntilItsLastFingerIsUp)
{
  CameraFingers fingers;

  fingers.press (finger, landing);
  fingers.press (secondFinger, landing);
  fingers.release (finger);
  EXPECT_FALSE (fingers.press (emulatedMouse, landing));
  EXPECT_EQ (fingers.count (), 1u);
  EXPECT_FLOAT_EQ (fingers.pinchDistance (), 0.f) << "one finger, no pinch";
}

// A release that never arrived must not lock the camera: fingers no longer
// down are dropped, and the next press is anyone's.
TEST (CameraFingers, ALostReleaseDoesNotLockTheCamera)
{
  CameraFingers fingers;

  fingers.press (finger, landing);
  fingers.forgetIfNotDown ([] (SourceKey) { return false; });
  EXPECT_EQ (fingers.count (), 0u);
  EXPECT_TRUE (fingers.press (emulatedMouse, landing));

  fingers.forgetIfNotDown ([] (SourceKey) { return true; });
  EXPECT_EQ (fingers.count (), 1u) << "still down, still counted";
}
