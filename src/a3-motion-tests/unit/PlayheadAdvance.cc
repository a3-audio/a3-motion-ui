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

#include <a3-motion-engine/Playhead.hh>

using namespace a3;

namespace
{

Playhead const middle{ 0.5f, 1.f, false };

TEST (PlayheadAdvance, InsideTheLoopItJustMovesOn)
{
  auto const next = advancePlayhead (middle, 0.1f, EndAction::Loop, 0.f);

  EXPECT_FLOAT_EQ (next.position, 0.6f);
  EXPECT_FLOAT_EQ (next.sign, 1.f);
  EXPECT_FALSE (next.stopped);
}

TEST (PlayheadAdvance, ForwardIsWhereReverseIsNot)
{
  EXPECT_FLOAT_EQ (initialSign (PlayDirection::Forward), 1.f);
  EXPECT_FLOAT_EQ (initialSign (PlayDirection::Reverse), -1.f);
}

TEST (PlayheadAdvance, ReverseWalksBackwards)
{
  Playhead const backwards{ 0.5f, -1.f, false };
  auto const next = advancePlayhead (backwards, 0.1f, EndAction::Loop, 0.f);

  EXPECT_FLOAT_EQ (next.position, 0.4f);
  EXPECT_FLOAT_EQ (next.sign, -1.f);
}

TEST (PlayheadAdvance, LoopWrapsAtEitherEnd)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const wrapped = advancePlayhead (nearEnd, 0.1f, EndAction::Loop, 0.f);
  EXPECT_NEAR (wrapped.position, 0.05f, 1e-5f);
  EXPECT_FALSE (wrapped.stopped);

  Playhead const nearStart{ 0.05f, -1.f, false };
  auto const under = advancePlayhead (nearStart, 0.1f, EndAction::Loop, 0.f);
  EXPECT_NEAR (under.position, 0.95f, 1e-5f);
  EXPECT_FLOAT_EQ (under.sign, -1.f);
}

// Bounce turns round rather than wrapping, and the overshoot is what it has
// already travelled back -- so it comes off the end at the same rate it
// arrived, not with a stumble.
TEST (PlayheadAdvance, BounceTurnsRoundAtTheEnd)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const turned = advancePlayhead (nearEnd, 0.1f, EndAction::Bounce, 0.f);

  EXPECT_NEAR (turned.position, 0.95f, 1e-5f);
  EXPECT_FLOAT_EQ (turned.sign, -1.f);
  EXPECT_FALSE (turned.stopped);
}

TEST (PlayheadAdvance, BounceTurnsRoundAtTheStartToo)
{
  Playhead const nearStart{ 0.05f, -1.f, false };
  auto const turned = advancePlayhead (nearStart, 0.1f, EndAction::Bounce, 0.f);

  EXPECT_NEAR (turned.position, 0.05f, 1e-5f);
  EXPECT_FLOAT_EQ (turned.sign, 1.f);
}

// Stop leaves the playhead exactly where it stood. The clip is taken out of
// playback by the caller, and the channel keeps the position it last had --
// which is what "the blob stays where it is" means.
TEST (PlayheadAdvance, StopHoldsThePositionItReached)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const halted = advancePlayhead (nearEnd, 0.1f, EndAction::Stop, 0.f);

  EXPECT_FLOAT_EQ (halted.position, 0.95f);
  EXPECT_TRUE (halted.stopped);
}

TEST (PlayheadAdvance, StopDoesNothingUntilTheEndIsReached)
{
  auto const next = advancePlayhead (middle, 0.1f, EndAction::Stop, 0.f);

  EXPECT_FLOAT_EQ (next.position, 0.6f);
  EXPECT_FALSE (next.stopped);
}

// Random carries on somewhere else. The phase is drawn by the caller so this
// stays a function that can be tested at all.
TEST (PlayheadAdvance, RandomCarriesOnAtTheDrawnPhase)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const jumped = advancePlayhead (nearEnd, 0.1f, EndAction::Random, 0.42f);

  EXPECT_FLOAT_EQ (jumped.position, 0.42f);
  EXPECT_FLOAT_EQ (jumped.sign, 1.f) << "it carries on the way it was going";
  EXPECT_FALSE (jumped.stopped);
}

TEST (PlayheadAdvance, RandomOnlyJumpsAtTheEnd)
{
  auto const next = advancePlayhead (middle, 0.1f, EndAction::Random, 0.42f);

  EXPECT_FLOAT_EQ (next.position, 0.6f) << "mid-loop nothing is drawn";
}

// A step longer than the whole loop must not throw the playhead outside it --
// a very fast clip advances by more than one pass per tick.
TEST (PlayheadAdvance, AHugeStepStaysInsideTheLoop)
{
  for (auto const action : { EndAction::Loop, EndAction::Bounce,
                             EndAction::Random })
    {
      auto const next = advancePlayhead (middle, 7.3f, action, 0.5f);
      EXPECT_GE (next.position, 0.f);
      EXPECT_LT (next.position, 1.f);
    }
}

}
