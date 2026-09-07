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

#include <set>

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/SphereProjection.hh>

#include <cmath>

using namespace a3;

namespace
{

// The display is an orthographic view of the upper hemisphere from above, and
// a blob is drawn by dropping z. So a drag has to invert exactly that, or the
// blob lands somewhere other than where the finger is — which is what dragging
// through the height map's pattern coordinates did: the finger's radius was
// read as a pattern radius, and 1.0 there is 45 degrees off the zenith rather
// than the horizon.

TEST (SphereProjection, TheCentreOfTheDiscIsStraightUp)
{
  auto const direction = discToDirection (Pos::fromCartesian (0.f, 0.f, 0.f));

  EXPECT_NEAR (direction.z (), 1.f, 0.001f);
}

TEST (SphereProjection, TheRimIsTheHorizon)
{
  auto const direction = discToDirection (Pos::fromCartesian (1.f, 0.f, 0.f));

  EXPECT_NEAR (direction.z (), 0.f, 0.001f);
  EXPECT_NEAR (direction.x (), 1.f, 0.001f);
}

TEST (SphereProjection, HalfWayOutIsSixtyDegreesUp)
{
  auto const direction = discToDirection (Pos::fromCartesian (0.5f, 0.f, 0.f));

  EXPECT_NEAR (direction.elevation (), 60.f, 0.5f);
}

// The whole point: what a drag writes and what the drawing reads back have to
// be the same place.
TEST (SphereProjection, DiscToDirectionAndBackIsTheIdentity)
{
  for (auto const point : { std::make_pair (0.3f, 0.2f),
                            std::make_pair (-0.7f, 0.1f),
                            std::make_pair (0.f, -0.95f) })
    {
      auto const flat = Pos::fromCartesian (point.first, point.second, 0.f);
      auto const back = directionToDisc (discToDirection (flat));

      EXPECT_NEAR (back.x (), point.first, 0.001f);
      EXPECT_NEAR (back.y (), point.second, 0.001f);
    }
}

TEST (SphereProjection, AzimuthSurvivesTheTrip)
{
  auto const flat = Pos::fromCartesian (0.4f, -0.6f, 0.f);
  auto const direction = discToDirection (flat);

  EXPECT_NEAR (direction.azimuth (), flat.azimuth (), 0.5f);
}

// A finger outside the sphere has no direction below the horizon to point at,
// so it is held at the rim rather than producing a NaN.
TEST (SphereProjection, BeyondTheRimIsHeldAtTheHorizon)
{
  auto const direction = discToDirection (Pos::fromCartesian (1.6f, 0.f, 0.f));

  EXPECT_NEAR (direction.z (), 0.f, 0.001f);
  EXPECT_NEAR (std::hypot (direction.x (), direction.y ()), 1.f, 0.001f);
}

}

// ── Drawn on the sphere, not through it ─────────────────────────────────

/** A step too long to be a straight line is drawn as the arc it is.
 *
 *  Near the pad's origin the projection moves without the disc moving, so two
 *  neighbouring samples can sit a fair way apart on the same latitude -- the
 *  two arms of a Clover's junction are nineteen degrees apart there. Joined by
 *  a straight line, that is a chord through the *inside* of the sphere, which
 *  is the one place the sound never is; drawn short, it reads as a line ruled
 *  across the picture. Walked along the sphere it is the small elbow it
 *  actually is.
 */
TEST (SphereProjection, AStepIsWalkedAlongTheSphereNotAcrossIt)
{
  auto const at = [] (float bearingDeg, float z) {
    auto const rad = bearingDeg * juce::MathConstants<float>::pi / 180.f;
    auto const rXY = std::sqrt (1.f - z * z);
    return Pos::fromCartesian (rXY * std::cos (rad), rXY * std::sin (rad), z);
  };

  auto const from = at (35.5f, 0.9177f);
  auto const to = at (54.5f, 0.9177f);

  for (int i = 0; i <= 8; ++i)
    {
      auto const t = static_cast<float> (i) / 8.f;
      auto const on = slerpDirection (from, to, t);

      EXPECT_NEAR (std::sqrt (on.x () * on.x () + on.y () * on.y ()
                              + on.z () * on.z ()),
                   1.f, 1e-4f)
          << "t " << t << ": the walk left the sphere";
      EXPECT_NEAR (on.z (), 0.9177f, 2e-3f)
          << "t " << t << ": it should stay at the height it started at";
    }

  // The ends are the ends, exactly: a walk that does not arrive is a gap.
  auto const start = slerpDirection (from, to, 0.f);
  auto const end = slerpDirection (from, to, 1.f);
  EXPECT_NEAR (start.x (), from.x (), 1e-5f);
  EXPECT_NEAR (end.y (), to.y (), 1e-5f);
}

// Two points that are already the same point have no arc between them, and
// asking for one must not divide by the sine of nothing.
TEST (SphereProjection, AWalkToWhereYouAlreadyAreIsNotANaN)
{
  auto const here = Pos::fromCartesian (0.f, 0.f, 1.f);
  auto const on = slerpDirection (here, here, 0.5f);

  EXPECT_NEAR (on.z (), 1.f, 1e-5f);
}

/** juce::PathFlatteningIterator::subPathIndex counts *line segments*, not
 *  sub-paths -- juce_PathIterator.cpp increments it on every line marker. The
 *  name says otherwise, and reading it as a sub-path index is what drew every
 *  tick-built trajectory as a few thousand two-point strokes instead of one
 *  line: a stroke that starts afresh at every tick is not joined to the one
 *  before it, so wherever two ticks land far apart -- the pad's origin -- the
 *  line simply stopped and started again.
 *
 *  Pinned here because the fix depends on the trap being real: if a later JUCE
 *  makes the member mean what it says, this fails and the workaround can go.
 */
TEST (SphereProjection, JucesSubPathIndexCountsSegmentsNotSubPaths)
{
  juce::Path path;
  path.startNewSubPath (0.f, 0.f);
  for (int i = 1; i <= 5; ++i)
    path.lineTo (static_cast<float> (i), 0.f);

  std::set<int> reported;
  juce::PathFlatteningIterator iter (path, {}, 0.005f);
  while (iter.next ())
    reported.insert (iter.subPathIndex);

  EXPECT_GT (reported.size (), 1u)
      << "subPathIndex now names sub-paths; the workaround can be removed";
}

/** So a new stroke is found the only way that is actually true of a path:
 *  this segment starts where the last one ended, or it does not. */
TEST (SphereProjection, AStrokeBreaksWhereTheSegmentsStopMeeting)
{
  juce::Path path;
  path.startNewSubPath (0.f, 0.f);
  path.lineTo (1.f, 0.f);
  path.lineTo (1.f, 1.f);
  path.startNewSubPath (5.f, 5.f);
  path.lineTo (6.f, 5.f);

  int breaks = 0;
  bool firstSegment = true;
  float prevX = 0.f, prevY = 0.f;

  juce::PathFlatteningIterator iter (path, {}, 0.005f);
  while (iter.next ())
    {
      if (firstSegment
          || std::abs (iter.x1 - prevX) > 1e-6f
          || std::abs (iter.y1 - prevY) > 1e-6f)
        ++breaks;

      firstSegment = false;
      prevX = iter.x2;
      prevY = iter.y2;
    }

  EXPECT_EQ (breaks, 2) << "one for the start, one for the second sub-path";
}

// ── The little sphere ───────────────────────────────────────────────────

/** It sits in the top right of the view, clear of it, and is big enough to
 *  take hold of. Turning the room is a thing you do with a finger, so a mark
 *  too small to land on is a mark that cannot do its job. */
TEST (SphereProjection, TheCameraBallSitsInTheTopRightAndCanBeGrabbed)
{
  for (int width : { 480, 768, 1024 })
    for (int height : { 400, 700, 900 })
      {
        juce::Rectangle<int> const view{ 0, 0, width, height };
        auto const ball = cameraBallBounds (view);

        ASSERT_FALSE (ball.isEmpty ()) << width << "x" << height;
        EXPECT_TRUE (view.contains (ball)) << width << "x" << height;
        EXPECT_GE (ball.getWidth (), fingertipSize) << width << "x" << height;
        EXPECT_EQ (ball.getWidth (), ball.getHeight ());

        // In the corner: nearer the top than the bottom, nearer the right
        // than the left.
        EXPECT_LT (ball.getCentreY (), view.getCentreY ());
        EXPECT_GT (ball.getCentreX (), view.getCentreX ());
      }
}

/** A view too small to hold one gets none rather than a ball drawn over the
 *  sphere it is meant to sit beside. */
TEST (SphereProjection, AViewTooSmallForTheBallGetsNone)
{
  EXPECT_TRUE (cameraBallBounds ({ 0, 0, 20, 20 }).isEmpty ());
  EXPECT_TRUE (cameraBallBounds ({}).isEmpty ());
}

/** Its own width is a whole turn and its own height a right angle, so one
 *  sweep across it has been all the way round the room.
 *
 *  And the room follows the finger: dragged right, the ball turns its front to
 *  the right, the way a globe under a hand does. Turned the other way it looks
 *  like the room is being pushed away rather than rolled. */
TEST (SphereProjection, ASweepAcrossTheBallIsAWholeTurn)
{
  juce::Rectangle<int> const ball{ 0, 0, 60, 60 };
  SphereCamera const overhead;

  auto const round = cameraFromBallDrag (overhead, { 60.f, 0.f }, ball);
  EXPECT_NEAR (round.turn, -juce::MathConstants<float>::twoPi, 1e-4f);
  EXPECT_NEAR (round.pitch, 0.f, 1e-4f);

  auto const over = cameraFromBallDrag (overhead, { 0.f, 60.f }, ball);
  EXPECT_NEAR (over.pitch, -juce::MathConstants<float>::halfPi, 1e-4f);
}

/** It tips both ways. One way meant that leaving the overhead view was a
 *  decision about which half of the room you would be able to look into, taken
 *  before you knew which one you wanted -- and the way back was to walk the
 *  long way round rather than to rock back through the view you started in. */
TEST (SphereProjection, TheBallTipsBothWays)
{
  juce::Rectangle<int> const ball{ 0, 0, 60, 60 };

  auto const one = cameraFromBallDrag ({}, { 0.f, 20.f }, ball).pitch;
  auto const other = cameraFromBallDrag ({}, { 0.f, -20.f }, ball).pitch;

  EXPECT_LT (one, 0.f);
  EXPECT_GT (other, 0.f);
  EXPECT_NEAR (one, -other, 1e-4f) << "the same finger, the same lean";
}

/** And stops at the horizon in each. Past a right angle the eye is under the
 *  floor looking up at it, which is not a view anybody is standing in. */
TEST (SphereProjection, TheBallWillNotTipPastTheHorizon)
{
  juce::Rectangle<int> const ball{ 0, 0, 60, 60 };

  EXPECT_NEAR (cameraFromBallDrag ({}, { 0.f, 300.f }, ball).pitch,
               -juce::MathConstants<float>::halfPi, 1e-4f);
  EXPECT_NEAR (cameraFromBallDrag ({}, { 0.f, -300.f }, ball).pitch,
               juce::MathConstants<float>::halfPi, 1e-4f);
}

/** A drag carries on from where the eye already was, so picking the ball up
 *  again does not throw away the view you had set. */
TEST (SphereProjection, ADragCarriesOnFromWhereTheEyeWas)
{
  juce::Rectangle<int> const ball{ 0, 0, 60, 60 };
  SphereCamera const leant{ 0.4f, 1.2f };

  auto const moved = cameraFromBallDrag (leant, { 15.f, 0.f }, ball);

  EXPECT_NEAR (moved.pitch, leant.pitch, 1e-4f);
  EXPECT_NEAR (moved.turn,
               leant.turn - juce::MathConstants<float>::twoPi / 4.f, 1e-4f);
}

/** The view settles onto the four bearings the ring is marked with. A view a
 *  few degrees off square is one whose four numbers all sit slightly wrong,
 *  and squaring it up by hand on a ball this size is finer work than a finger
 *  can do. */
TEST (SphereProjection, TheViewSettlesOntoTheMarkedBearings)
{
  auto const quarter = juce::MathConstants<float>::halfPi;

  for (int step = -4; step <= 4; ++step)
    {
      auto const square = static_cast<float> (step) * quarter;

      EXPECT_NEAR (cameraSettled ({ 0.f, square + 0.05f }).turn, square, 1e-5f)
          << "step " << step;
      EXPECT_NEAR (cameraSettled ({ 0.f, square - 0.05f }).turn, square, 1e-5f)
          << "step " << step;
    }
}

/** And a view deliberately set between two of them stays where it was put. */
TEST (SphereProjection, AViewSetBetweenBearingsIsLeftThere)
{
  auto const between = juce::MathConstants<float>::halfPi / 2.f;

  EXPECT_NEAR (cameraSettled ({ 0.f, between }).turn, between, 1e-5f);
}

/** The lean is not settled: the overhead view is one end of its range and the
 *  horizon the other, and both are reached by running out of ball. */
TEST (SphereProjection, TheLeanIsLeftAlone)
{
  EXPECT_NEAR (cameraSettled ({ 0.3f, 0.f }).pitch, 0.3f, 1e-5f);
}
