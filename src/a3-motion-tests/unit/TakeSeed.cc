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

#include <a3-motion-engine/KnobLanes.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TakeSeed.hh>

using namespace a3;

namespace
{
Pos
at (float x)
{
  return Pos::fromCartesian (x, 0.f, 0.f);
}
}

// Overdub: a take on a slot that holds a clip starts from that clip, so TOUCH
// changes only what is touched -- pots turned over the old figure, or one
// corner of it mended.

TEST (TakeSeed, ThePathIsCarriedOverAtTheTakesLength)
{
  Pattern from;
  from.resize (8);
  for (index_t t = 0; t < 8; ++t)
    from.setTick (t, at (0.1f * static_cast<float> (t)));

  Pattern take;
  take.resize (16);
  seedTake (take, from);

  EXPECT_NEAR (take.getTick (0).x (), 0.0f, 1e-5f);
  EXPECT_NEAR (take.getTick (6).x (), 0.3f, 1e-5f);
  EXPECT_NEAR (take.getTick (15).x (), 0.7f, 1e-5f);
}

TEST (TakeSeed, TheCarriedPathCountsAsWritten)
{
  // Or closing the seams would fill it over with holds.
  Pattern from;
  from.resize (4);
  for (index_t t = 0; t < 4; ++t)
    from.setTick (t, at (0.2f));

  Pattern take;
  take.resize (4);
  seedTake (take, from);

  for (auto const written : take.writtenTicks ())
    EXPECT_TRUE (written);
}

TEST (TakeSeed, AGapStaysAGap)
{
  Pattern from;
  from.resize (4);
  from.setTick (0, at (0.2f));
  from.setTick (1, at (0.3f));

  Pattern take;
  take.resize (4);
  seedTake (take, from);

  auto const written = take.writtenTicks ();
  EXPECT_TRUE (written[1]);
  EXPECT_FALSE (written[2]);
  EXPECT_FALSE (written[3]);
}

TEST (TakeSeed, TheLanesAreCarriedOver)
{
  Pattern from;
  from.resize (8);
  KnobLanes lanes;
  auto &reach = lanes[static_cast<std::size_t> (Knob::Reach)];
  reach = KnobLane (8);
  reach.write (0, 0.2f);
  reach.write (4, 0.7f);
  from.setLanes (lanes);

  Pattern take;
  take.resize (16);
  seedTake (take, from);

  auto const carried = take.getLanes ()[static_cast<std::size_t> (Knob::Reach)];
  EXPECT_EQ (carried.ticks (), 16);
  EXPECT_FLOAT_EQ (carried.at (7.0).value_or (-1.f), 0.2f);
  EXPECT_FLOAT_EQ (carried.at (9.0).value_or (-1.f), 0.7f);
}
