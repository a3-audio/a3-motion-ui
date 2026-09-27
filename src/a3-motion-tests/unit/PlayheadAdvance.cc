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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/Playhead.hh>

using namespace a3;

namespace
{

Playhead const middle{ 0.5f, 1.f, false };

TEST (PlayheadAdvance, InsideTheLoopItJustMovesOn)
{
  auto const next = advancePlayhead (middle, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f);

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
  auto const next = advancePlayhead (backwards, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f);

  EXPECT_FLOAT_EQ (next.position, 0.4f);
  EXPECT_FLOAT_EQ (next.sign, -1.f);
}

TEST (PlayheadAdvance, LoopWrapsAtEitherEnd)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const wrapped = advancePlayhead (nearEnd, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f);
  EXPECT_NEAR (wrapped.position, 0.05f, 1e-5f);
  EXPECT_FALSE (wrapped.stopped);

  Playhead const nearStart{ 0.05f, -1.f, false };
  auto const under = advancePlayhead (nearStart, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f);
  EXPECT_NEAR (under.position, 0.95f, 1e-5f);
  EXPECT_FLOAT_EQ (under.sign, -1.f);
}

// Bounce turns round rather than wrapping, and the overshoot is what it has
// already travelled back -- so it comes off the end at the same rate it
// arrived, not with a stumble.
TEST (PlayheadAdvance, BounceTurnsRoundAtTheEnd)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const turned = advancePlayhead (nearEnd, 0.1f, PlayDirection::Bounce, EndAction::Loop, 0.f);

  EXPECT_NEAR (turned.position, 0.95f, 1e-5f);
  EXPECT_FLOAT_EQ (turned.sign, -1.f);
  EXPECT_FALSE (turned.stopped);
}

TEST (PlayheadAdvance, BounceTurnsRoundAtTheStartToo)
{
  Playhead const nearStart{ 0.05f, -1.f, false };
  auto const turned = advancePlayhead (nearStart, 0.1f, PlayDirection::Bounce, EndAction::Loop, 0.f);

  EXPECT_NEAR (turned.position, 0.05f, 1e-5f);
  EXPECT_FLOAT_EQ (turned.sign, 1.f);
}

// Pause leaves the playhead exactly where it stood. The clip is taken out of
// playback by the caller, and the channel keeps the position it last had --
// which is what "the blob stays where it is" means.
//
// This used to be what Stop did, and it was the only thing on offer: standing
// still where you happen to land is a pause, and calling it a stop left no way
// to ask for the other one.
TEST (PlayheadAdvance, PauseHoldsThePositionItReached)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const halted = advancePlayhead (nearEnd, 0.1f, PlayDirection::Forward, EndAction::Pause, 0.f);

  EXPECT_FLOAT_EQ (halted.position, 0.95f);
  EXPECT_TRUE (halted.stopped);
}

TEST (PlayheadAdvance, StopDoesNothingUntilTheEndIsReached)
{
  auto const next = advancePlayhead (middle, 0.1f, PlayDirection::Forward, EndAction::Stop, 0.f);

  EXPECT_FLOAT_EQ (next.position, 0.6f);
  EXPECT_FALSE (next.stopped);
}

// Random carries on somewhere else. The phase is drawn by the caller so this
// stays a function that can be tested at all.
TEST (PlayheadAdvance, RandomCarriesOnAtTheDrawnPhase)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };
  auto const jumped = advancePlayhead (nearEnd, 0.1f, PlayDirection::Random, EndAction::Loop, 0.42f);

  EXPECT_FLOAT_EQ (jumped.position, 0.42f);
  EXPECT_FLOAT_EQ (jumped.sign, 1.f) << "it carries on the way it was going";
  EXPECT_FALSE (jumped.stopped);
}

TEST (PlayheadAdvance, RandomOnlyJumpsAtTheEnd)
{
  auto const next = advancePlayhead (middle, 0.1f, PlayDirection::Random, EndAction::Loop, 0.42f);

  EXPECT_FLOAT_EQ (next.position, 0.6f) << "mid-loop nothing is drawn";
}

// A step longer than the whole loop must not throw the playhead outside it --
// a very fast clip advances by more than one pass per tick.
TEST (PlayheadAdvance, AHugeStepStaysInsideTheLoop)
{
  for (auto const direction : { PlayDirection::Forward, PlayDirection::Bounce,
                                PlayDirection::Random })
    {
      auto const next
          = advancePlayhead (middle, 7.3f, direction, EndAction::Loop, 0.5f);
      EXPECT_GE (next.position, 0.f);
      EXPECT_LE (next.position, 1.f);
    }
}

// Choosing a direction has to turn a clip that is already playing. The sign
// used to be set only when playback started, so a clip kept running the way it
// had set off until it was stopped and started again.
TEST (PlayheadAdvance, ChoosingADirectionTurnsAClipAtOnce)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setPlaySign (1.f);

  pattern.setPlayDirection (PlayDirection::Reverse);
  EXPECT_FLOAT_EQ (pattern.getPlaySign (), -1.f);

  pattern.setPlayDirection (PlayDirection::Forward);
  EXPECT_FLOAT_EQ (pattern.getPlaySign (), 1.f);
}

}


// Stop is a stop: the pass ends and the playhead goes back to where the take
// begins, so the next start is visibly a start. That was what "stop" did on
// every deck the maintainer has stood behind, and what this one did was hold
// still where it happened to land — which is a pause.
TEST (PlayheadAdvance, StopEndsThePassAtTheBeginning)
{
  auto const stepped = advancePlayhead ({ 0.99f, 1.f, false }, 0.02f,
                                        PlayDirection::Forward, EndAction::Stop, 0.f);

  EXPECT_TRUE (stepped.stopped);
  EXPECT_FLOAT_EQ (stepped.position, 0.f);
}

// Running backwards it is still the beginning it goes to, not the end it came
// from: "back to the start" is about the take, not about the direction.
TEST (PlayheadAdvance, StopGoesToTheBeginningWhicheverWayItWasGoing)
{
  auto const stepped = advancePlayhead ({ 0.01f, -1.f, false }, 0.02f,
                                        PlayDirection::Forward, EndAction::Stop, 0.f);

  EXPECT_TRUE (stepped.stopped);
  EXPECT_FLOAT_EQ (stepped.position, 0.f);
}

// And what the old Stop did keeps a name of its own, because it is a useful
// thing: hold where it got to, and start again from there.
TEST (PlayheadAdvance, PauseHoldsWhereItGotTo)
{
  auto const stepped = advancePlayhead ({ 0.99f, 1.f, false }, 0.02f,
                                        PlayDirection::Forward, EndAction::Pause, 0.f);

  EXPECT_TRUE (stepped.stopped);
  EXPECT_FLOAT_EQ (stepped.position, 0.99f);
}

// A step that lands exactly on the end is the ordinary case, not a corner one:
// the delta is one tick's share of the pass, so a clip whose playback length
// matches its take arrives on 1.0 dead on. Reflecting it gave 1.0, and wrapping
// that into the pass turned the turn into a teleport back to the take's first
// tick -- travelling the whole shape in one tick, at the same spot every time.
TEST (PlayheadAdvance, BounceTurningExactlyOnTheEndStaysAtTheEnd)
{
  Playhead const atTheEdge{ 0.9f, 1.f, false };
  auto const turned = advancePlayhead (atTheEdge, 0.1f, PlayDirection::Bounce, EndAction::Loop, 0.f);

  EXPECT_FLOAT_EQ (turned.position, 1.f);
  EXPECT_FLOAT_EQ (turned.sign, -1.f);
  EXPECT_FALSE (turned.stopped);

  // And away from it on the next tick, rather than sticking there.
  auto const away = advancePlayhead (turned, 0.1f, PlayDirection::Bounce, EndAction::Loop, 0.f);
  EXPECT_FLOAT_EQ (away.position, 0.9f);
  EXPECT_FLOAT_EQ (away.sign, -1.f);
}

// The other end turns one tick later, and that is correct rather than
// asymmetric: zero is a position the take has, so a step landing on it stands
// on the take's first tick and turns from there. Both ends are visited exactly
// once per pass -- 0.9, 1.0, 0.9 at one end and 0.1, 0.0, 0.1 at the other --
// which is what a turn should look like.
TEST (PlayheadAdvance, BounceStandsOnTheStartBeforeTurning)
{
  Playhead const arriving{ 0.1f, -1.f, false };
  auto const onIt = advancePlayhead (arriving, 0.1f, PlayDirection::Bounce, EndAction::Loop, 0.f);

  EXPECT_FLOAT_EQ (onIt.position, 0.f);
  EXPECT_FLOAT_EQ (onIt.sign, -1.f) << "not turned yet: it is standing on it";

  auto const away = advancePlayhead (onIt, 0.1f, PlayDirection::Bounce, EndAction::Loop, 0.f);
  EXPECT_FLOAT_EQ (away.position, 0.1f);
  EXPECT_FLOAT_EQ (away.sign, 1.f);
  EXPECT_GE (away.position, 0.f) << "it must never fall through the start";
}

// ── Finishing the pass on purpose ────────────────────────────────────────────
//
// Pressing play on a running clip used to stop it on the next beat, in the
// middle of whatever figure it was drawing. It finishes the pass now and stops
// at its end -- so the three "carry on" end actions are overruled for that one
// lap, which is the whole point: Loop, Bounce and Random are exactly the
// clips a person wants to get *out* of at a musical boundary.

TEST (PlayheadAdvance, StopAtEndChangesNothingInsideThePass)
{
  auto const next = advancePlayhead (middle, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f, true);

  EXPECT_FLOAT_EQ (next.position, 0.6f);
  EXPECT_FALSE (next.stopped);
}

TEST (PlayheadAdvance, StopAtEndEndsALoopWhereTheLapEnds)
{
  Playhead const nearlyThere{ 0.95f, 1.f, false };

  auto const looping
      = advancePlayhead (nearlyThere, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f, false);
  EXPECT_FALSE (looping.stopped);

  auto const finishing
      = advancePlayhead (nearlyThere, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f, true);
  EXPECT_TRUE (finishing.stopped);

  // Back at the take's start, the way EndAction::Stop leaves it: this was
  // asked for at a boundary, and the next press should be a start.
  EXPECT_FLOAT_EQ (finishing.position, 0.f);
}

// A bounce is asked to finish at the end of its whole round -- out and back --
// not at the far end it turns at (the maintainer's call, 2026-09-26): half a
// round is half a figure.
TEST (PlayheadAdvance, StopAtEndLetsABounceFinishItsRound)
{
  Playhead const atTheFarEnd{ 0.95f, 1.f, false };
  auto const turned = advancePlayhead (atTheFarEnd, 0.1f, PlayDirection::Bounce,
                                       EndAction::Loop, 0.f, true);
  EXPECT_FALSE (turned.stopped) << "it stopped halfway round";
  EXPECT_FLOAT_EQ (turned.sign, -1.f);

  Playhead const nearlyHome{ 0.05f, -1.f, false };
  auto const home = advancePlayhead (nearlyHome, 0.1f, PlayDirection::Bounce,
                                     EndAction::Loop, 0.f, true);
  EXPECT_TRUE (home.stopped);
  EXPECT_FLOAT_EQ (home.position, 0.f);
}

TEST (PlayheadAdvance, StopAtEndCatchesARandomJumpToo)
{
  Playhead const nearlyThere{ 0.95f, 1.f, false };

  auto const finishing
      = advancePlayhead (nearlyThere, 0.1f, PlayDirection::Random, EndAction::Loop, 0.42f, true);
  EXPECT_TRUE (finishing.stopped);
  EXPECT_FLOAT_EQ (finishing.position, 0.f);
}

TEST (PlayheadAdvance, StopAtEndAddsNothingToAClipThatStopsAnyway)
{
  Playhead const nearlyThere{ 0.95f, 1.f, false };

  auto const byItself
      = advancePlayhead (nearlyThere, 0.1f, PlayDirection::Forward, EndAction::Stop, 0.f, false);
  auto const asked
      = advancePlayhead (nearlyThere, 0.1f, PlayDirection::Forward, EndAction::Stop, 0.f, true);

  EXPECT_EQ (byItself.stopped, asked.stopped);
  EXPECT_FLOAT_EQ (byItself.position, asked.position);
}

// Whichever way it was running. "Back to the start" is about the take, not
// about the direction -- the same rule EndAction::Stop already follows.
TEST (PlayheadAdvance, StopAtEndCatchesAReverseLapAtItsEnd)
{
  Playhead const nearlyBack{ 0.05f, -1.f, false };

  auto const finishing
      = advancePlayhead (nearlyBack, 0.1f, PlayDirection::Forward, EndAction::Loop, 0.f, true);

  EXPECT_TRUE (finishing.stopped);
  EXPECT_FLOAT_EQ (finishing.position, 0.f);
}

// ── Direction and end, combined (2026-09-26) ────────────────────────────────
//
// Bounce and Random moved from the end actions to the directions: they say
// how a clip travels, and the end -- Loop, Stop, Pause -- says what it does
// when that travel is over. A bounce's travel is its whole round, out and
// back; a random one's is a lap from wherever it was dropped in.

TEST (PlayheadAdvance, ABounceTurnsAtTheFarEndWhateverItsEnd)
{
  Playhead const atTheFarEnd{ 0.95f, 1.f, false };
  for (auto const end : { EndAction::Loop, EndAction::Stop, EndAction::Pause })
    {
      auto const turned = advancePlayhead (atTheFarEnd, 0.1f,
                                           PlayDirection::Bounce, end, 0.f);
      EXPECT_FALSE (turned.stopped);
      EXPECT_FLOAT_EQ (turned.sign, -1.f);
    }
}

TEST (PlayheadAdvance, ABounceThatStopsStopsWhenItIsHome)
{
  Playhead const nearlyHome{ 0.05f, -1.f, false };
  auto const home = advancePlayhead (nearlyHome, 0.1f, PlayDirection::Bounce,
                                     EndAction::Stop, 0.f);

  EXPECT_TRUE (home.stopped);
  EXPECT_FLOAT_EQ (home.position, 0.f);
  EXPECT_FLOAT_EQ (home.sign, 1.f) << "the next start sets off outwards";
}

TEST (PlayheadAdvance, ABounceThatPausesHoldsWhereItGotHome)
{
  Playhead const nearlyHome{ 0.05f, -1.f, false };
  auto const held = advancePlayhead (nearlyHome, 0.1f, PlayDirection::Bounce,
                                     EndAction::Pause, 0.f);

  EXPECT_TRUE (held.stopped);
  EXPECT_FLOAT_EQ (held.position, 0.05f);
}

TEST (PlayheadAdvance, ARandomLapThatStopsDoesNotJump)
{
  Playhead const nearEnd{ 0.95f, 1.f, false };

  auto const stopped = advancePlayhead (nearEnd, 0.1f, PlayDirection::Random,
                                        EndAction::Stop, 0.42f);
  EXPECT_TRUE (stopped.stopped);
  EXPECT_FLOAT_EQ (stopped.position, 0.f);

  auto const paused = advancePlayhead (nearEnd, 0.1f, PlayDirection::Random,
                                       EndAction::Pause, 0.42f);
  EXPECT_TRUE (paused.stopped);
  EXPECT_FLOAT_EQ (paused.position, 0.95f);
}

// Where a clip sets off from: the start forwards, the end backwards, a random
// phase at random, and a bounce outwards from the start.
TEST (PlayheadAdvance, EachDirectionSetsOffFromItsOwnPlace)
{
  EXPECT_FLOAT_EQ (initialPosition (PlayDirection::Forward, 0.42f), 0.f);
  EXPECT_FLOAT_EQ (initialPosition (PlayDirection::Reverse, 0.42f), 1.f);
  EXPECT_FLOAT_EQ (initialPosition (PlayDirection::Bounce, 0.42f), 0.f);
  EXPECT_FLOAT_EQ (initialPosition (PlayDirection::Random, 0.42f), 0.42f);

  EXPECT_FLOAT_EQ (initialSign (PlayDirection::Bounce), 1.f);
  EXPECT_FLOAT_EQ (initialSign (PlayDirection::Random), 1.f);
}

// Every direction and end is written by a name and read back by it.
TEST (PlayheadAdvance, TheNamesRoundTrip)
{
  for (auto const direction : { PlayDirection::Forward, PlayDirection::Reverse,
                                PlayDirection::Bounce, PlayDirection::Random })
    EXPECT_EQ (playDirectionFromName (playDirectionToName (direction)),
               direction);

  for (auto const end : { EndAction::Loop, EndAction::Stop, EndAction::Pause })
    EXPECT_EQ (endActionFromName (endActionToName (end)), end);
}

// A clip written when Bounce and Random were end actions keeps playing the
// way it did: the end it named becomes its direction, and it loops.
TEST (PlayheadAdvance, AnOldBounceOrRandomEndBecomesADirection)
{
  auto const bounce = playbackModeFromNames ("fwd", "bounce");
  EXPECT_EQ (bounce.direction, PlayDirection::Bounce);
  EXPECT_EQ (bounce.endAction, EndAction::Loop);

  auto const random = playbackModeFromNames ("rev", "random");
  EXPECT_EQ (random.direction, PlayDirection::Random);
  EXPECT_EQ (random.endAction, EndAction::Loop);

  auto const plain = playbackModeFromNames ("rev", "stop");
  EXPECT_EQ (plain.direction, PlayDirection::Reverse);
  EXPECT_EQ (plain.endAction, EndAction::Stop);

  auto const unknown = playbackModeFromNames ("", "");
  EXPECT_EQ (unknown.direction, PlayDirection::Forward);
  EXPECT_EQ (unknown.endAction, EndAction::Loop);
}

// A pattern set to bounce or to random sets off forwards.
TEST (PlayheadAdvance, BounceAndRandomSetOffForwards)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setPlaySign (-1.f);

  pattern.setPlayDirection (PlayDirection::Bounce);
  EXPECT_FLOAT_EQ (pattern.getPlaySign (), 1.f);
}

// Only a clip that runs off one end and on at the other travels the step from
// the take's last tick to its first -- forwards or backwards, looping. A
// bounce turns before it, a random lap jumps it, and a clip that stops never
// gets there.
TEST (PlayheadAdvance, OnlyALoopRunningStraightTravelsTheWrap)
{
  EXPECT_TRUE (travelsTheWrap (PlayDirection::Forward, EndAction::Loop));
  EXPECT_TRUE (travelsTheWrap (PlayDirection::Reverse, EndAction::Loop));
  EXPECT_FALSE (travelsTheWrap (PlayDirection::Bounce, EndAction::Loop));
  EXPECT_FALSE (travelsTheWrap (PlayDirection::Random, EndAction::Loop));
  EXPECT_FALSE (travelsTheWrap (PlayDirection::Forward, EndAction::Stop));
  EXPECT_FALSE (travelsTheWrap (PlayDirection::Forward, EndAction::Pause));
}

// The channel row's bar is time through the clip's length, not where the
// playhead stands in the figure: under Random every pass starts at a random
// point, so the position runs passes of random length (2026-09-27, the bar
// "filled unevenly"). The lap is counted in whole ticks, one per tick, and
// wraps at the length whatever the direction does.
TEST (PlayheadAdvance, TheLapCountsEveryTickAndWrapsAtTheLength)
{
  constexpr index_t length = 512;
  index_t tick = 0;
  for (int i = 0; i < 3 * 512 + 100; ++i)
    tick = nextLapTick (tick, length);
  EXPECT_EQ (tick, 100u);

  EXPECT_EQ (nextLapTick (length - 1, length), 0u);
  EXPECT_FLOAT_EQ (lapProgress (256, length), 0.5f);
}

// A length turned shorter mid-lap still lands inside the new one.
TEST (PlayheadAdvance, AShorterLengthPutsTheLapBackInside)
{
  EXPECT_LT (nextLapTick (400, 256), 256u);
  EXPECT_EQ (nextLapTick (7, 0), 0u);
  EXPECT_FLOAT_EQ (lapProgress (7, 0), 0.f);
}

// Random drops in at a random point at every pass end: that is a jump the
// blob is to glide over (2026-09-27), so the playhead says so.
TEST (PlayheadAdvance, ARandomPassEndSaysItJumped)
{
  auto const stepped
      = advancePlayhead ({ 0.999f, 1.f, false }, 0.01f, PlayDirection::Random,
                         EndAction::Loop, 0.4f);
  EXPECT_TRUE (stepped.jumped);
  EXPECT_FLOAT_EQ (stepped.position, 0.4f);
}

// A step on, and a loop running over its seam, are not jumps: the seam is the
// bridges' to join (planBridges).
TEST (PlayheadAdvance, AStepAndALoopSeamAreNoJump)
{
  EXPECT_FALSE (advancePlayhead ({ 0.5f, 1.f, false }, 0.01f,
                                 PlayDirection::Random, EndAction::Loop, 0.4f)
                    .jumped);
  EXPECT_FALSE (advancePlayhead ({ 0.999f, 1.f, false }, 0.01f,
                                 PlayDirection::Forward, EndAction::Loop, 0.4f)
                    .jumped);
}
