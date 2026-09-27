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

#include <a3-motion-engine/Glide.hh>

using namespace a3;

namespace
{
float
length (Pos p)
{
  return std::hypot (p.x (), p.y (), p.z ());
}

float
distance (Pos a, Pos b)
{
  return std::hypot (a.x () - b.x (), a.y () - b.y (), a.z () - b.z ());
}

Pos const front = Pos::fromCartesian (1.f, 0.f, 0.f);
Pos const left = Pos::fromCartesian (0.f, 1.f, 0.f);
}

// Rnd always glides (2026-09-27): a beat at least, and GAP-CONNECTOR's fade
// lengthens it up to half a pass, the time a bridge may reserve wide open. A
// pass shorter than two beats glides half of it.
TEST (Glide, AGlideTakesABeatAndTheFadeLengthensIt)
{
  constexpr index_t beat = 64;
  EXPECT_EQ (glideTicks (0.f, 512, beat), beat);
  EXPECT_EQ (glideTicks (0.5f, 512, beat), 128u);
  EXPECT_EQ (glideTicks (1.f, 512, beat), 256u);
  EXPECT_EQ (glideTicks (2.f, 512, beat), 256u) << "clamped";
  EXPECT_EQ (glideTicks (0.f, 64, beat), 32u) << "a short pass";
}

// No glide under way: the blob is where the playhead puts it.
TEST (Glide, WithoutAGlideTheBlobIsOnTheLine)
{
  Glide glide;
  auto const at = glidedPosition (glide, left);
  EXPECT_FLOAT_EQ (distance (at, left), 0.f);
}

// A glide leaves from where the blob was and arrives on the running line when
// its ticks are spent -- and not before.
TEST (Glide, AGlideLeavesFromTheBlobAndLandsOnTheLine)
{
  Glide glide = startGlide (front, 10);
  auto const first = glidedPosition (glide, left);
  EXPECT_LT (distance (first, front), distance (first, left));

  Pos at = first;
  for (int i = 1; i < 10; ++i)
    at = glidedPosition (glide, left);
  EXPECT_NEAR (distance (at, left), 0.f, 1e-5f);
  EXPECT_FLOAT_EQ (distance (glidedPosition (glide, left), left), 0.f)
      << "and stays there";
}

// Along the sphere, not through the room: half way between two points on it
// the blob is still on it, not under the listener's nose.
TEST (Glide, AGlideRunsAlongTheSphere)
{
  Glide glide = startGlide (front, 2);
  auto const half = glidedPosition (glide, left);
  EXPECT_NEAR (length (half), 1.f, 1e-4f);
}

// Two points opposite each other have no one great circle; the glide still
// ends where it should and never passes the middle of the room.
TEST (Glide, OppositePointsStillGlideRoundTheSphere)
{
  auto const back = Pos::fromCartesian (-1.f, 0.f, 0.f);
  Glide glide = startGlide (front, 4);
  Pos at = front;
  for (int i = 0; i < 4; ++i)
    {
      at = glidedPosition (glide, back);
      EXPECT_GT (length (at), 0.9f) << "tick " << i;
    }
  EXPECT_NEAR (distance (at, back), 0.f, 1e-5f);
}
