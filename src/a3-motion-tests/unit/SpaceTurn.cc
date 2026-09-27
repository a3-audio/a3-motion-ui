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

// What a clip leans by right now: its setting, swept out and back by its
// own sweep the way a squeeze is by its stretch.
TEST (SpaceTurn, AClipLeansByItsSettingAndItsSweep)
{
  Pattern pattern;
  pattern.setTilt (0.5f);
  pattern.setRoll (-0.25f);

  auto const still = spaceTurnOf (pattern);
  EXPECT_FLOAT_EQ (still.tilt, 0.5f);
  EXPECT_FLOAT_EQ (still.roll, -0.25f);

  pattern.setTiltLfo (2);
  pattern.setTiltLfoPhase (0.3f);
  EXPECT_FLOAT_EQ (spaceTurnOf (pattern).tilt,
                   lfoSweepBipolar (0.5f, 2, 0.3f));
}
