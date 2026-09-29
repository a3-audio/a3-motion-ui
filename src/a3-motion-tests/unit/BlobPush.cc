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

// #56 (maintainer, 2026-09-29): a blob pushed aside by a held one moves on
// the screen only -- its channel stays where it is in the room. The push is
// an offset in the sphere's 2D pixel space; these are its rules.

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/BlobPush.hh>

using namespace a3;

namespace
{
using P = juce::Point<float>;
constexpr float radius = 50.f;
}

TEST (BlobPush, ABlobUnderTheHeldOneIsPushedOutToTheRadius)
{
  P const blob{ 10.f, 0.f };
  auto const offset = nextPushOffset (blob, {}, { P{ 0.f, 0.f } }, radius);
  auto const drawn = blob + offset;
  EXPECT_NEAR (drawn.getDistanceFrom ({ 0.f, 0.f }), radius + 1.f, 0.01f);
  EXPECT_GT (drawn.x, blob.x) << "pushed away from the held one, not across it";
}

TEST (BlobPush, ABlobFarAwayIsNotPushed)
{
  auto const offset
      = nextPushOffset ({ 200.f, 0.f }, {}, { P{ 0.f, 0.f } }, radius);
  EXPECT_EQ (offset, P{});
}

TEST (BlobPush, APushedBlobDriftsBackWhenTheHeldOneLeaves)
{
  P const blob{ 10.f, 0.f };
  P offset{ 41.f, 0.f };
  for (int tick = 0; tick < 200; ++tick)
    offset = nextPushOffset (blob, offset, { P{ -300.f, 0.f } }, radius);
  EXPECT_LT (offset.getDistanceFromOrigin (), 41.f * 0.5f)
      << "at least half way home after 200 ticks";
}

TEST (BlobPush, EveryHeldBlobPushes)
{
  P const blob{ 0.f, 0.f };
  auto const offset = nextPushOffset (
      blob, {}, { P{ -10.f, 0.f }, P{ 0.f, 200.f } }, radius);
  EXPECT_GE ((blob + offset).getDistanceFrom ({ -10.f, 0.f }), radius);
}

TEST (BlobPush, LettingGoEasesThePushAway)
{
  P offset{ 40.f, -30.f };
  auto const first = easedPushOffset (offset);
  EXPECT_LT (first.getDistanceFromOrigin (), offset.getDistanceFromOrigin ());
  for (int tick = 0; tick < 200; ++tick)
    offset = easedPushOffset (offset);
  EXPECT_EQ (offset, P{}) << "and ends at exactly nothing, not a crawl";
}
