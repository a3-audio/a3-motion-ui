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

#include <a3-motion-ui/components/TickPlayheads.hh>

using namespace a3;

namespace
{

juce::Rectangle<float> const tick{ 100.f, 10.f, 200.f, 20.f };
constexpr float width = 2.f;

// A channel that is not playing draws nothing. The array the status bar is
// handed always has one entry per channel, so "not playing" has to be a value
// rather than an absence -- and every drawing site has to agree on which.
TEST (TickPlayheads, AChannelThatIsNotPlayingHasNoPlayhead)
{
  EXPECT_TRUE (playheadBounds (tick, -1.f, width).isEmpty ());
}

TEST (TickPlayheads, TheStartOfAClipSitsAtTheLeftEdge)
{
  auto const head = playheadBounds (tick, 0.f, width);

  EXPECT_FLOAT_EQ (head.getX (), tick.getX ());
  EXPECT_FLOAT_EQ (head.getWidth (), width);
}

// The end of a clip is the right edge of the indicator, not two pixels past
// it: a playhead is drawn *over* the tick indicator, so one hanging off the
// end would paint on the status bar beside it.
TEST (TickPlayheads, TheEndOfAClipStaysInsideTheIndicator)
{
  auto const head = playheadBounds (tick, 1.f, width);

  EXPECT_FLOAT_EQ (head.getRight (), tick.getRight ());
  EXPECT_LE (head.getX (), tick.getRight () - width);
}

TEST (TickPlayheads, HalfWayThroughIsHalfWayAcross)
{
  auto const head = playheadBounds (tick, 0.5f, width);

  EXPECT_NEAR (head.getCentreX (), tick.getCentreX (), width);
}

// The playhead keeps the indicator's height: it is a mark across the beats,
// not a box floating in the middle of them.
TEST (TickPlayheads, ThePlayheadSpansTheIndicatorsHeight)
{
  auto const head = playheadBounds (tick, 0.25f, width);

  EXPECT_FLOAT_EQ (head.getY (), tick.getY ());
  EXPECT_FLOAT_EQ (head.getHeight (), tick.getHeight ());
}

// A position past the end is what an engine one tick ahead of the repaint
// hands over. Clamped rather than refused: the alternative is a playhead that
// blinks out for a frame at the loop point, which reads as a dropped clip.
TEST (TickPlayheads, APositionPastTheEndIsClampedRatherThanDropped)
{
  auto const head = playheadBounds (tick, 1.4f, width);

  EXPECT_FALSE (head.isEmpty ());
  EXPECT_FLOAT_EQ (head.getRight (), tick.getRight ());
}


// The engine plays a clip backwards by counting its position down: `dir` sets
// a sign of -1 and a bounce flips it at the ends. Followed literally, the mark
// then travels right to left, and at a bounce it turns round mid-run.
//
// It always sweeps left to right instead. The indicator says how far, not
// which way -- a mark that sometimes runs backwards has to be read rather
// than caught out of the corner of an eye, which is the one thing it is for.
TEST (TickPlayheads, ForwardPlaybackIsShownAsItIs)
{
  EXPECT_FLOAT_EQ (leftToRightPosition (0.25f, 1.f), 0.25f);
}

TEST (TickPlayheads, BackwardPlaybackIsMirroredSoItStillSweepsRight)
{
  EXPECT_FLOAT_EQ (leftToRightPosition (0.75f, -1.f), 0.25f);
  EXPECT_FLOAT_EQ (leftToRightPosition (1.f, -1.f), 0.f);
}

// A bounce turns round at the end of the take. Mirrored, the mark leaves the
// right edge and comes back in at the left -- the same picture a loop makes,
// which is what "always left to right" has to mean for a clip that reverses.
TEST (TickPlayheads, ABounceReadsAsALoopRatherThanAReversal)
{
  auto const beforeTurn = leftToRightPosition (0.99f, 1.f);
  auto const afterTurn = leftToRightPosition (0.99f, -1.f);

  EXPECT_GT (beforeTurn, 0.9f);
  EXPECT_LT (afterTurn, 0.1f);
}

// Not playing stays not playing: the sentinel has to survive the mirroring,
// or a stopped channel would come out at the right-hand edge instead of
// drawing nothing.
TEST (TickPlayheads, TheNotPlayingSentinelSurvivesMirroring)
{
  EXPECT_LT (leftToRightPosition (-1.f, -1.f), 0.f);
  EXPECT_LT (leftToRightPosition (-1.f, 1.f), 0.f);
}


// The property the mirroring exists for, stated outright: how far the mark
// still has to travel is how much of the clip is still to play. That holds in
// both directions, which is why a backward clip is mirrored rather than drawn
// as it runs -- the indicator answers "how much longer", and an answer that
// means two different things depending on `dir` is not an answer.
TEST (TickPlayheads, TheDistanceToTheRightEdgeIsWhatIsLeftToPlay)
{
  // Forward at 0.75: a quarter of the take is left.
  auto const forward = playheadBounds (tick, leftToRightPosition (0.75f, 1.f),
                                       width);
  EXPECT_NEAR ((tick.getRight () - forward.getRight ()) / tick.getWidth (),
               0.25f, 0.02f);

  // Backward at 0.75: it counts down to zero, so three quarters are left.
  auto const backward = playheadBounds (tick, leftToRightPosition (0.75f, -1.f),
                                        width);
  EXPECT_NEAR ((tick.getRight () - backward.getRight ()) / tick.getWidth (),
               0.75f, 0.02f);
}

}
