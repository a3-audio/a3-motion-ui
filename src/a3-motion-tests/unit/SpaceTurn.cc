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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/TempoLfo.hh>

#include <cmath>

using namespace a3;

namespace
{
void
expectNear (Pos const &a, Pos const &b)
{
  EXPECT_NEAR (a.x (), b.x (), 1e-5f);
  EXPECT_NEAR (a.y (), b.y (), 1e-5f);
  EXPECT_NEAR (a.z (), b.z (), 1e-5f);
}
}

// Tilt and roll lean the figure's plane in the room (2026-09-27), after it is
// on the sphere: tilt about the left-right axis, roll about the front-back
// one, each a quarter turn at its end. x is front, y left, z up.

TEST (SpaceTurn, NoTurnLeavesADirectionAlone)
{
  auto const d = Pos::fromCartesian (0.3f, -0.4f, 0.866f);
  expectNear (turnedInSpace (d, {}), d);
}

TEST (SpaceTurn, AFullTiltTakesTheFrontDown)
{
  expectNear (turnedInSpace (Pos::fromCartesian (1.f, 0.f, 0.f), { 1.f, 0.f }),
              Pos::fromCartesian (0.f, 0.f, -1.f));
  // The axis it turns about stays where it is.
  expectNear (turnedInSpace (Pos::fromCartesian (0.f, 1.f, 0.f), { 1.f, 0.f }),
              Pos::fromCartesian (0.f, 1.f, 0.f));
}

TEST (SpaceTurn, AFullRollTakesTheLeftDown)
{
  expectNear (turnedInSpace (Pos::fromCartesian (0.f, 1.f, 0.f), { 0.f, 1.f }),
              Pos::fromCartesian (0.f, 0.f, -1.f));
  expectNear (turnedInSpace (Pos::fromCartesian (1.f, 0.f, 0.f), { 0.f, 1.f }),
              Pos::fromCartesian (1.f, 0.f, 0.f));
}

// The finger draws in the leant plane: where it lands is turned back before
// it is written, so the take plays back under the finger.
TEST (SpaceTurn, UnturningUndoesTheTurn)
{
  SpaceTurn const turn{ 0.37f, -0.62f };
  for (auto const &d : { Pos::fromCartesian (0.3f, -0.4f, 0.866f),
                         Pos::fromCartesian (-0.7f, 0.1f, 0.707f),
                         Pos::fromCartesian (0.f, 0.f, 1.f) })
    expectNear (unturnedInSpace (turnedInSpace (d, turn), turn), d);
}

TEST (SpaceTurn, NothingStaysNothing)
{
  EXPECT_FALSE (turnedInSpace (Pos::invalid, { 0.5f, 0.5f }).isValid ());
  EXPECT_FALSE (unturnedInSpace (Pos::invalid, { 0.5f, 0.5f }).isValid ());
}

// What a clip leans by right now: its setting, turned on round by its own
// sweep the way rot is by spin (2026-09-28) -- not out and back any more.

namespace
{
/** Two leans are the same when they differ by whole turns: -2 and 2 are both
 *  upside down. */
void
expectSameLean (float a, float b)
{
  auto const apart = std::remainder (a - b, 4.f);
  // The tick-by-tick phase carries a thousandth of a cycle of float
  // rounding over its longest cycle (TempoLfo's own tolerance), which is four
  // thousandths of a quarter turn here.
  EXPECT_NEAR (apart, 0.f, 5e-3f) << a << " vs " << b;
}

constexpr float ticksPerBar = 384.f;

/** How far a tilt sweep of `step` has carried a clip after `bars`, ticked the
 *  way the engine ticks it. */
float
tiltAfter (float tilt, int step, float bars)
{
  Pattern pattern;
  pattern.setTilt (tilt);
  pattern.setTiltLfo (step);
  auto phase = 0.f;
  auto const ticks = std::lround (bars * ticksPerBar);
  for (long t = 0; t < ticks; ++t)
    phase = advanceLfoPhase (phase, step, ticksPerBar);
  pattern.setTiltLfoPhase (phase);
  return spaceTurnOf (pattern).tilt;
}
}

TEST (SpaceTurn, AClipStandingStillLeansByItsSetting)
{
  Pattern pattern;
  pattern.setTilt (0.5f);
  pattern.setRoll (-0.25f);

  auto const still = spaceTurnOf (pattern);
  EXPECT_FLOAT_EQ (still.tilt, 0.5f);
  EXPECT_FLOAT_EQ (still.roll, -0.25f);
}

// A lean is an angle on a closed ring, in quarter turns: -2 and 2 are the
// same upside-down plane, and a value past either wraps rather than stops.
TEST (SpaceTurn, ALeanComesRoundToItself)
{
  EXPECT_FLOAT_EQ (wrappedLean (0.f), 0.f);
  EXPECT_FLOAT_EQ (wrappedLean (1.f), 1.f);
  EXPECT_FLOAT_EQ (wrappedLean (-1.75f), -1.75f);
  EXPECT_FLOAT_EQ (wrappedLean (2.f), -2.f);
  EXPECT_FLOAT_EQ (wrappedLean (2.5f), -1.5f);
  EXPECT_FLOAT_EQ (wrappedLean (-2.25f), 1.75f);
  EXPECT_FLOAT_EQ (wrappedLean (9.f), 1.f);
  // Bit for bit on the ring: a saved lean must come back as it was written.
  EXPECT_EQ (wrappedLean (0.35f), 0.35f);
  EXPECT_EQ (wrappedLean (-0.45f), -0.45f);

  Pattern pattern;
  pattern.setTilt (2.5f);
  pattern.setRoll (-3.f);
  EXPECT_FLOAT_EQ (pattern.getTilt (), -1.5f);
  EXPECT_FLOAT_EQ (pattern.getRoll (), 1.f);
}

// Everything a clip could hold before (-1..1, a quarter turn at the ends)
// is the same angle now: a saved clip with its sweep at 0 sounds as it did.
TEST (SpaceTurn, AnOldLeanKeepsItsAngle)
{
  for (auto const v : { -1.f, -0.6f, 0.f, 0.3f, 1.f })
    {
      Pattern pattern;
      pattern.setTilt (v);
      pattern.setRoll (v);
      EXPECT_FLOAT_EQ (spaceTurnOf (pattern).tilt, v);
      EXPECT_FLOAT_EQ (spaceTurnOf (pattern).roll, v);
    }
}

// Past a quarter turn is still a rigid turn of the room: a half turn of tilt
// puts the front at the back and the top at the bottom.
TEST (SpaceTurn, AHalfTiltTurnsTheFigureOver)
{
  expectNear (turnedInSpace (Pos::fromCartesian (0.f, 0.f, 1.f), { 2.f, 0.f }),
              Pos::fromCartesian (0.f, 0.f, -1.f));
  expectNear (turnedInSpace (Pos::fromCartesian (1.f, 0.f, 0.f), { 2.f, 0.f }),
              Pos::fromCartesian (-1.f, 0.f, 0.f));
  expectNear (turnedInSpace (Pos::fromCartesian (0.f, 1.f, 0.f), { 0.f, -2.f }),
              Pos::fromCartesian (0.f, -1.f, 0.f));
}

// The sweep goes round, not out and back: three quarters of the way through
// its cycle the plane has turned three quarters, where an out-and-back sweep
// would be on its way home at the quarter's value.
TEST (SpaceTurn, ATiltSweepTurnsRoundRatherThanRocking)
{
  Pattern pattern;
  pattern.setTilt (0.5f);
  pattern.setTiltLfo (2);

  pattern.setTiltLfoPhase (0.25f);
  expectSameLean (spaceTurnOf (pattern).tilt, 1.5f);
  pattern.setTiltLfoPhase (0.5f);
  expectSameLean (spaceTurnOf (pattern).tilt, 2.5f);
  pattern.setTiltLfoPhase (0.75f);
  expectSameLean (spaceTurnOf (pattern).tilt, 3.5f);

  auto const lean = spaceTurnOf (pattern).tilt;
  EXPECT_GE (lean, -2.f);
  EXPECT_LT (lean, 2.f) << "a reader gets an angle, not a running total";
}

TEST (SpaceTurn, ARollSweepTurnsRoundToo)
{
  Pattern pattern;
  pattern.setRoll (-0.5f);
  pattern.setRollLfo (-3);
  pattern.setRollLfoPhase (0.25f);
  expectSameLean (spaceTurnOf (pattern).roll, 0.5f);
  EXPECT_FLOAT_EQ (spaceTurnOf (pattern).tilt, 0.f)
      << "one axis' sweep leaves the other alone";
}

// One revolution per the step's bars, on the spin's table: |1| is 32 bars,
// |8| a quarter bar. Half-way it is upside down; at the end it is back.
TEST (SpaceTurn, ATiltSweepTurnsOnceInItsBars)
{
  for (int step = 1; step <= lfoMaxStep; ++step)
    {
      auto const bars = lfoBarsPerCycle (step);
      expectSameLean (tiltAfter (0.25f, step, bars * 0.25f), 1.25f);
      expectSameLean (tiltAfter (0.25f, step, bars * 0.5f), 2.25f);
      expectSameLean (tiltAfter (0.25f, step, bars), 0.25f);
    }
}

// The sign is the direction: positive takes the front down, negative takes
// it up -- a quarter of the cycle is a quarter turn either way.
TEST (SpaceTurn, TheSignOfASweepIsItsDirection)
{
  expectSameLean (tiltAfter (0.f, 4, lfoBarsPerCycle (4) * 0.25f), 1.f);
  expectSameLean (tiltAfter (0.f, -4, lfoBarsPerCycle (4) * 0.25f), -1.f);
}

// A sweep at 0 turns nothing -- not even by the phase a stopped sweep left
// behind, the rule turnsOf() has for the spin.
TEST (SpaceTurn, AStoppedSweepTurnsNothing)
{
  Pattern pattern;
  pattern.setTilt (0.5f);
  pattern.setRoll (0.25f);
  pattern.setTiltLfoPhase (0.3f);
  pattern.setRollLfoPhase (0.6f);

  EXPECT_FLOAT_EQ (spaceTurnOf (pattern).tilt, 0.5f);
  EXPECT_FLOAT_EQ (spaceTurnOf (pattern).roll, 0.25f);
  EXPECT_FLOAT_EQ (tiltAfter (0.5f, 0, 8.f), 0.5f);
}
