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
#include <a3-motion-engine/Playhead.hh>

#include <cmath>

using namespace a3;

namespace
{
constexpr index_t numTicks = 64;

/** An open path: a straight run from left to right. Its last tick is nowhere
 *  near its first, so the seam between them is the widest step in the take by
 *  a long way -- which is what makes it useful for asking whether playback
 *  ever crosses it. */
void
aStraightRun (Pattern &pattern)
{
  pattern.resize (numTicks);
  for (index_t tick = 0; tick < numTicks; ++tick)
    {
      auto const t = static_cast<float> (tick)
                     / static_cast<float> (numTicks - 1);
      pattern.setTick (tick, Pos::fromCartesian (-0.8f + 1.6f * t, 0.f, 0.f));
    }
  pattern.markComplete ();
}

float
distanceTo (Pos const &a, Pos const &b)
{
  return std::hypot (a.x () - b.x (), a.y () - b.y ());
}
}

// Bounce turns round at the end of the take. The end of the take is its last
// tick -- not the wrap point one tick further on, where the take's end is
// joined to its beginning.
//
// Sampling over the full tick count puts that seam inside the playable range,
// so a bouncing clip runs a little way along the join before it turns: on an
// open path the blob darts off towards the start and comes back. Every one of
// the four generated bounce presets was rejected by ear, with "springt an
// einer stelle" written against one of them.
TEST (BouncePlayback, TheTurnDoesNotRunIntoTheSeam)
{
  Pattern pattern;
  aStraightRun (pattern);

  auto const interior = 1.6f / static_cast<float> (numTicks - 1);

  Playhead head{ 0.5f, 1.f, false };
  auto const delta = 1.f / static_cast<float> (numTicks);

  auto previous = pattern.getInterpolatedTick (
      fractionalTickForPlayback (head.position, numTicks, EndAction::Bounce));

  auto worst = 0.f;
  // Far enough to cross the end and come back past where it started.
  for (int step = 0; step < 2 * static_cast<int> (numTicks); ++step)
    {
      head = advancePlayhead (head, delta, EndAction::Bounce, 0.f);
      auto const here = pattern.getInterpolatedTick (fractionalTickForPlayback (
          head.position, numTicks, EndAction::Bounce));
      worst = std::max (worst, distanceTo (previous, here));
      previous = here;
    }

  // A little over one interior step is the most a turn can cost: the reflected
  // step is the same length as the one that arrived.
  EXPECT_LT (worst, interior * 1.5f)
      << "the blob travelled " << worst << " in one tick, where a step inside "
      << "the take is " << interior << " -- the turn ran into the seam";
}

// Looping is the opposite case and must keep its seam: the take's end IS
// joined to its beginning there, and the fade exists to smooth exactly that
// join. Sampling a looping clip over anything less than the full tick count
// would drop the last tick out of the loop.
TEST (BouncePlayback, LoopingStillReachesTheSeam)
{
  EXPECT_FLOAT_EQ (fractionalTickForPlayback (0.f, numTicks, EndAction::Loop),
                   0.f);
  EXPECT_NEAR (fractionalTickForPlayback (0.999f, numTicks, EndAction::Loop),
               static_cast<double> (numTicks) * 0.999, 1e-4);
}

// The turning points are the take's own ends, exactly.
TEST (BouncePlayback, BounceSpansTheTicksThemselves)
{
  EXPECT_FLOAT_EQ (fractionalTickForPlayback (0.f, numTicks, EndAction::Bounce),
                   0.f);
  EXPECT_NEAR (fractionalTickForPlayback (1.f, numTicks, EndAction::Bounce),
               static_cast<double> (numTicks - 1), 1e-6);
}

// One tick is a standing still, not a division by zero.
TEST (BouncePlayback, ASingleTickIsNotDividedBy)
{
  EXPECT_FLOAT_EQ (fractionalTickForPlayback (0.7f, 1, EndAction::Bounce), 0.f);
}
