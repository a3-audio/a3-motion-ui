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

#include <a3-motion-engine/flight/FlightBodiesBox.hh>

#include <atomic>
#include <thread>

using namespace a3;

// The bodies the DJ placed reach the clock thread through a SeqLock: the
// message thread writes, the clock thread copies, and neither ever waits for
// the other. A copy caught half-written is refused, never retried, so the
// clock thread keeps last tick's bodies instead of spinning.

namespace
{
/** Every field of every body equals `n`: a snapshot that mixes two writes
 *  shows up as two different numbers. */
FlightBodies
allEqualTo (int n)
{
  FlightBodies bodies;
  bodies.count = maxFlightBodies;
  for (auto &body : bodies.body)
    body = { { static_cast<float> (n), static_cast<float> (n) },
             static_cast<float> (n), n };
  return bodies;
}

bool
isOneSnapshot (FlightBodies const &bodies)
{
  if (bodies.count != maxFlightBodies)
    return false;
  auto const n = bodies.body[0].id;
  for (auto const &body : bodies.body)
    if (body.id != n || body.at.x != static_cast<float> (n)
        || body.at.y != static_cast<float> (n)
        || body.mass != static_cast<float> (n))
      return false;
  return true;
}
}

TEST (FlightBodiesBox, WhatIsWrittenIsRead)
{
  FlightBodiesBox box;
  FlightBodies written;
  written.count = 2;
  written.body[0] = { { 0.3f, -0.2f }, 2.f, 7 };
  written.body[1] = { { -0.5f, 0.4f }, -2.f, 9 };
  box.write (written);

  FlightBodies read;
  ASSERT_TRUE (box.read (read));
  EXPECT_EQ (read.count, 2);
  EXPECT_EQ (read.body[0].at, written.body[0].at);
  EXPECT_FLOAT_EQ (read.body[0].mass, 2.f);
  EXPECT_EQ (read.body[0].id, 7);
  EXPECT_EQ (read.body[1].at, written.body[1].at);
  EXPECT_FLOAT_EQ (read.body[1].mass, -2.f);
  EXPECT_EQ (read.body[1].id, 9);
}

TEST (FlightBodiesBox, NothingWrittenReadsAnEmptyFloor)
{
  FlightBodiesBox box;
  FlightBodies read = allEqualTo (3);
  ASSERT_TRUE (box.read (read));
  EXPECT_EQ (read.count, 0);
}

TEST (FlightBodiesBox, ATornReadIsRefusedNotSpun)
{
  FlightBodiesBox box;
  box.write (allEqualTo (1));

  box.beginWrite ();
  FlightBodies read = allEqualTo (5);
  EXPECT_FALSE (box.read (read)) << "a copy taken mid-write was accepted";
  EXPECT_TRUE (isOneSnapshot (read));
  EXPECT_EQ (read.body[0].id, 5) << "a refused read touched the caller's copy";
  box.endWrite ();

  ASSERT_TRUE (box.read (read));
  EXPECT_EQ (read.body[0].id, 1);
}

TEST (FlightBodiesBox, ThreadsAgree)
{
  constexpr int snapshots = 100000;
  FlightBodiesBox box;
  box.write (allEqualTo (0));

  std::atomic<bool> done{ false };
  std::thread writer ([&] {
    for (int n = 1; n <= snapshots; ++n)
      box.write (allEqualTo (n));
    done = true;
  });

  long long accepted = 0, mixed = 0, backwards = 0;
  int last = 0;
  FlightBodies read;
  while (!done.load ())
    {
      if (!box.read (read))
        continue;
      ++accepted;
      if (!isOneSnapshot (read))
        ++mixed;
      else if (read.body[0].id < last)
        ++backwards;
      else
        last = read.body[0].id;
    }
  writer.join ();

  EXPECT_EQ (mixed, 0) << "the reader saw a snapshot from two writes";
  EXPECT_EQ (backwards, 0) << "the reader went back to an older snapshot";
  EXPECT_GT (accepted, 0);
  ASSERT_TRUE (box.read (read));
  EXPECT_EQ (read.body[0].id, snapshots) << "the newest write did not win";
}
