/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-ui/components/fpv/FloorGesture.hh>

using namespace a3;

namespace
{
constexpr float blob = 40.f; // slop = 20 px
constexpr int body = 3;

FloorGesture
gesture ()
{
  FloorGesture g;
  g.setBlobDiameter (blob);
  return g;
}
}

TEST (FloorGesture, AStillShortTouchOnEmptyFloorPlaces)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., std::nullopt);
  EXPECT_EQ (g.move ({ 103.f, 101.f }, 50.), FloorAction::None);
  EXPECT_EQ (g.held (100.), FloorAction::None);
  EXPECT_EQ (g.up ({ 103.f, 101.f }, 150.), FloorAction::Place);
}

TEST (FloorGesture, AStillShortTouchOnABodyCyclesIt)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., body);
  EXPECT_EQ (g.body (), body);
  EXPECT_EQ (g.held (200.), FloorAction::None);
  EXPECT_EQ (g.up ({ 102.f, 100.f }, 250.), FloorAction::CycleWeight);
}

TEST (FloorGesture, MovingFromEmptyFloorIsCamera)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., std::nullopt);
  EXPECT_EQ (g.move ({ 130.f, 100.f }, 50.), FloorAction::Camera);
  EXPECT_EQ (g.move ({ 101.f, 100.f }, 80.), FloorAction::Camera)
      << "decided once, kept even back at the start";
  EXPECT_EQ (g.held (2000.), FloorAction::None);
  EXPECT_EQ (g.up ({ 101.f, 100.f }, 2100.), FloorAction::None);
}

TEST (FloorGesture, MovingFromABodyDragsIt)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., body);
  EXPECT_EQ (g.move ({ 100.f, 125.f }, 50.), FloorAction::Drag);
  EXPECT_EQ (g.move ({ 100.f, 300.f }, 80.), FloorAction::Drag);
  EXPECT_EQ (g.held (5000.), FloorAction::None)
      << "a body being dragged and then held still is not removed";
  EXPECT_EQ (g.up ({ 100.f, 300.f }, 5100.), FloorAction::None);
  EXPECT_EQ (g.body (), body);
}

TEST (FloorGesture, HoldingABodyRemovesItOnceAndTheLiftDoesNothingMore)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., body);
  EXPECT_EQ (g.held (FloorGesture::longPressMs - 1.), FloorAction::None);
  EXPECT_EQ (g.held (FloorGesture::longPressMs), FloorAction::Remove);
  EXPECT_EQ (g.held (FloorGesture::longPressMs + 100.), FloorAction::None);
  EXPECT_EQ (g.move ({ 200.f, 100.f }, 800.), FloorAction::None)
      << "the body is gone; nothing left to drag";
  EXPECT_EQ (g.up ({ 200.f, 100.f }, 900.), FloorAction::None);
}

// The 30-Hz check may not come round before the finger lifts: a lift past the
// hold still removes, so the outcome does not depend on the timer's phase.
TEST (FloorGesture, ALiftPastTheHoldRemovesWhenTheCheckMissedIt)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., body);
  EXPECT_EQ (g.up ({ 100.f, 100.f }, FloorGesture::longPressMs + 10.),
             FloorAction::Remove);
}

TEST (FloorGesture, HoldingEmptyFloorDoesNothing)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., std::nullopt);
  EXPECT_EQ (g.held (FloorGesture::longPressMs * 3.), FloorAction::None);
  EXPECT_EQ (g.up ({ 100.f, 100.f }, FloorGesture::longPressMs * 3.),
             FloorAction::None)
      << "a long press is not a tap: no group is placed";
}

TEST (FloorGesture, TheSlopIsRelativeToTheBlob)
{
  FloorGesture large;
  large.setBlobDiameter (80.f);
  large.down ({ 0.f, 0.f }, 0., std::nullopt);
  EXPECT_EQ (large.move ({ 30.f, 0.f }, 10.), FloorAction::None);
  EXPECT_EQ (large.up ({ 30.f, 0.f }, 20.), FloorAction::Place);

  FloorGesture small;
  small.setBlobDiameter (20.f);
  small.down ({ 0.f, 0.f }, 0., std::nullopt);
  EXPECT_EQ (small.move ({ 30.f, 0.f }, 10.), FloorAction::Camera);
}

// The filling ring round a held body: 0 at the touch, 1 when it goes.
TEST (FloorGesture, TheHoldFillsTowardsTheRemoval)
{
  auto g = gesture ();
  EXPECT_FLOAT_EQ (g.holdProgress (0.), 0.f) << "no finger";
  g.down ({ 100.f, 100.f }, 1000., body);
  EXPECT_FLOAT_EQ (g.holdProgress (1000.), 0.f);
  EXPECT_NEAR (g.holdProgress (1000. + FloorGesture::longPressMs / 2.), 0.5f,
               1e-5f);
  EXPECT_FLOAT_EQ (g.holdProgress (1000. + FloorGesture::longPressMs * 2.),
                   1.f);
  g.move ({ 150.f, 100.f }, 1100.);
  EXPECT_FLOAT_EQ (g.holdProgress (1200.), 0.f) << "a drag shows no ring";
}

TEST (FloorGesture, EmptyFloorShowsNoRing)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., std::nullopt);
  EXPECT_FLOAT_EQ (g.holdProgress (300.), 0.f);
}

// A second finger (a pinch) takes an undecided touch away: nothing placed,
// cycled or removed when the first one lifts.
TEST (FloorGesture, ACancelledTouchDoesNothing)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., body);
  g.cancel ();
  EXPECT_EQ (g.held (FloorGesture::longPressMs * 2.), FloorAction::None);
  EXPECT_EQ (g.move ({ 200.f, 100.f }, 700.), FloorAction::None);
  EXPECT_EQ (g.up ({ 200.f, 100.f }, 800.), FloorAction::None);
  EXPECT_FLOAT_EQ (g.holdProgress (300.), 0.f);
}

TEST (FloorGesture, ANewTouchStartsAfresh)
{
  auto g = gesture ();
  g.down ({ 100.f, 100.f }, 0., body);
  g.held (FloorGesture::longPressMs);
  g.up ({ 100.f, 100.f }, 700.);
  g.down ({ 10.f, 10.f }, 1000., std::nullopt);
  EXPECT_FALSE (g.body ().has_value ());
  EXPECT_EQ (g.up ({ 10.f, 10.f }, 1100.), FloorAction::Place);
}

TEST (FloorGesture, ASwipeLiftedBeforeAnyMoveIsNotATap)
{
  // A fast finger can reach the lift with no move event in between.
  auto g = gesture ();
  g.down ({ 0.f, 0.f }, 0., std::nullopt);
  EXPECT_EQ (g.up ({ 100.f, 0.f }, 80.), FloorAction::None);

  g.down ({ 0.f, 0.f }, 0., body);
  EXPECT_EQ (g.up ({ 100.f, 0.f }, 80.), FloorAction::None);
}
