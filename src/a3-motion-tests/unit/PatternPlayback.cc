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

#include <a3-motion-engine/Pattern.hh>

#include <cmath>

using namespace a3;

namespace
{

/** Four positions, each held for a quarter of the loop: a take tapped in. */
void
tapFourCorners (Pattern &pattern, index_t numTicks = 512)
{
  std::vector<Pos> const corners
      = { Pos::fromCartesian (0.6f, 0.6f, 0.f),
          Pos::fromCartesian (-0.6f, 0.6f, 0.f),
          Pos::fromCartesian (-0.6f, -0.6f, 0.f),
          Pos::fromCartesian (0.6f, -0.6f, 0.f) };

  pattern.resize (numTicks);
  for (index_t tick = 0; tick < numTicks; ++tick)
    pattern.setTick (tick,
                     corners[static_cast<size_t> (tick / (numTicks / 4))]);
  pattern.markComplete ();
}

float
distanceTo (Pos const &a, Pos const &b)
{
  return std::hypot (a.x () - b.x (), a.y () - b.y ());
}

// Playing a tapped take means standing at each tap and jumping to the next. It
// used to slide: getInterpolatedTick interpolates between neighbouring ticks
// whatever lies between them, so across a tap's single-tick jump the blob was
// drawn at every point along the way -- never quite on the tap, and a tap held
// only briefly was crossed without ever being reached.
TEST (PatternPlayback, ATappedTakeIsPlayedAsTapsNotAsASlide)
{
  Pattern pattern;
  tapFourCorners (pattern);

  std::vector<Pos> const corners
      = { Pos::fromCartesian (0.6f, 0.6f, 0.f),
          Pos::fromCartesian (-0.6f, 0.6f, 0.f),
          Pos::fromCartesian (-0.6f, -0.6f, 0.f),
          Pos::fromCartesian (0.6f, -0.6f, 0.f) };

  int between = 0;
  for (int sample = 0; sample < 2000; ++sample)
    {
      auto const at = 512.0 * sample / 2000.0;
      auto const here = pattern.getInterpolatedTick (at);

      auto nearest = 10.f;
      for (auto const &corner : corners)
        nearest = std::min (nearest, distanceTo (here, corner));

      if (nearest > 0.01f)
        ++between;
    }

  EXPECT_EQ (between, 0)
      << between << " of 2000 samples sat between two taps rather than on one";
}

// The same thing at the loop point, which is where it was noticed: coming round
// from the last tap the blob set off towards the first one early, so it moved
// while it should have been standing still.
TEST (PatternPlayback, TheLastTapIsHeldUntilTheLoopPoint)
{
  Pattern pattern;
  tapFourCorners (pattern);

  auto const last = pattern.getTick (500);
  for (double at = 384.0; at < 511.9; at += 0.5)
    EXPECT_LT (distanceTo (pattern.getInterpolatedTick (at), last), 0.01f)
        << "at " << at;
}

// A drawn trajectory must keep its smoothness -- holding between every pair of
// ticks would make an even motion into a staircase.
TEST (PatternPlayback, ADrawnTakeIsStillInterpolated)
{
  Pattern pattern;
  pattern.resize (64);
  for (index_t tick = 0; tick < 64; ++tick)
    {
      auto const a = juce::MathConstants<float>::twoPi * tick / 64.f;
      pattern.setTick (tick, Pos::fromCartesian (std::cos (a) * 0.6f,
                                                 std::sin (a) * 0.6f, 0.f));
    }
  pattern.markComplete ();

  auto const onTick = pattern.getInterpolatedTick (10.0);
  auto const halfway = pattern.getInterpolatedTick (10.5);
  auto const nextTick = pattern.getInterpolatedTick (11.0);

  EXPECT_GT (distanceTo (halfway, onTick), 0.001f)
      << "the motion stopped being interpolated and became a staircase";
  EXPECT_GT (distanceTo (halfway, nextTick), 0.001f);
}

}
