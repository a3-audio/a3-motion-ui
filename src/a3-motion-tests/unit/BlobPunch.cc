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

#include <a3-motion-ui/components/BlobPunch.hh>

using namespace a3;

namespace
{
constexpr float frame = 1.f / 60.f;

float
hold (BlobPunch &punch, float peak, float seconds)
{
  float last = 0.f;
  for (float t = 0.f; t < seconds; t += frame)
    last = punch.update (peak, frame);
  return last;
}
}

TEST (BlobPunch, SilenceIsNoPunch)
{
  BlobPunch punch;
  EXPECT_FLOAT_EQ (hold (punch, 0.f, 2.f), 0.f);
}

// A steady tone is a level, not a hit, however loud.
TEST (BlobPunch, ASteadyToneSettlesToNothing)
{
  BlobPunch punch;
  EXPECT_LT (hold (punch, 0.9f, 3.f), 0.05f);
}

TEST (BlobPunch, AHitAfterAQuietBarPunchesHard)
{
  BlobPunch punch;
  hold (punch, 0.02f, 1.f);
  EXPECT_GT (punch.update (0.6f, frame), 0.8f);
}

TEST (BlobPunch, ABiggerJumpPunchesHarder)
{
  BlobPunch soft, hard;
  hold (soft, 0.05f, 1.f);
  hold (hard, 0.05f, 1.f);
  EXPECT_LT (soft.update (0.1f, frame), hard.update (0.6f, frame));
}

// It is a strike: gone again well before the next beat.
TEST (BlobPunch, ThePunchFallsAwayBetweenHits)
{
  BlobPunch punch;
  hold (punch, 0.02f, 1.f);
  punch.update (0.6f, frame);
  EXPECT_LT (hold (punch, 0.02f, 0.25f), 0.2f);
}

// Kicks at 128 BPM over a bed of music: every beat is a punch, the space
// between them is not -- which is what the random firing never managed.
TEST (BlobPunch, EveryBeatOfAGrooveIsAPunch)
{
  BlobPunch punch;
  auto const beat = 60.f / 128.f;
  hold (punch, 0.08f, 2.f);
  for (int i = 0; i < 8; ++i)
    {
      EXPECT_GT (punch.update (0.9f, frame), 0.5f) << "beat " << i;
      EXPECT_LT (hold (punch, 0.08f, beat - frame), 0.2f) << "after beat " << i;
    }
}
