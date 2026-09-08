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

}
