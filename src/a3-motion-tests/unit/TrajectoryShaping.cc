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

#include <cmath>

#include <JuceHeader.h>

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>
#include <a3-motion-engine/TrajectorySpin.hh>

using namespace a3;

// A clip nobody has touched has to play exactly as it was recorded. Both
// squeezes sit at their middle, and the middle of a multiplying control is
// one -- not zero, which is the trap a plain "scale = value" would fall into.
TEST (TrajectoryShaping, TheMiddleOfBothPotsLeavesTheTakeAlone)
{
  PlaneShaping shaping;

  Pos const points[] = { Pos::fromCartesian (1.f, 0.f, 0.f),
                         Pos::fromCartesian (0.f, 0.6f, 0.f),
                         Pos::fromCartesian (-0.3f, 0.4f, 0.f) };

  for (auto const &p : points)
    {
      auto const shaped = shapedPosition (p, shaping);
      EXPECT_NEAR (shaped.x (), p.x (), 1e-5f);
      EXPECT_NEAR (shaped.y (), p.y (), 1e-5f);
    }
}

// Which pot moves which axis, pinned as coordinates rather than as a picture.
// The screen mirrors these -- cartesian2DHOA2JUCE puts an HOA position at
// { -y, -x } -- so sqzX is the screen's *vertical* and sqzY its horizontal.
// A scale survives that mirror unharmed (negating both sides of a
// multiplication changes nothing), which is why this needs no sign convention
// the way spinPosition() does.
TEST (TrajectoryShaping, EachPotWorksOnItsOwnAxisAndLeavesTheOtherAlone)
{
  auto const p = Pos::fromCartesian (0.5f, 0.5f, 0.f);

  PlaneShaping onlyX;
  onlyX.squeezeX = 1.f;
  auto const shapedX = shapedPosition (p, onlyX);
  EXPECT_NEAR (shapedX.x (), 1.f, 1e-5f);
  EXPECT_NEAR (shapedX.y (), 0.5f, 1e-5f);

  PlaneShaping onlyY;
  onlyY.squeezeY = -1.f;
  auto const shapedY = shapedPosition (p, onlyY);
  EXPECT_NEAR (shapedY.x (), 0.5f, 1e-5f);
  EXPECT_NEAR (shapedY.y (), 0.25f, 1e-5f);
}

// Half at one end, double at the other, and the same distance turned either
// way undoes itself. A pot whose ends were 0 and 2 would have its unity point
// at three quarters of the travel, and there would be no telling by feel where
// "as recorded" is.
TEST (TrajectoryShaping, TheEndsOfTheTravelAreHalfAndDouble)
{
  EXPECT_NEAR (squeezeFactor (0.f), 1.f, 1e-6f);
  EXPECT_NEAR (squeezeFactor (1.f), 2.f, 1e-6f);
  EXPECT_NEAR (squeezeFactor (-1.f), 0.5f, 1e-6f);

  for (float value : { 0.25f, 0.5f, 0.75f })
    EXPECT_NEAR (squeezeFactor (value) * squeezeFactor (-value), 1.f, 1e-6f);
}

// Past the ends there is nothing: a value that arrives from a script or a
// file cannot stretch the take until it piles up on the far pole.
TEST (TrajectoryShaping, PastTheEndsItStops)
{
  EXPECT_NEAR (squeezeFactor (4.f), 2.f, 1e-6f);
  EXPECT_NEAR (squeezeFactor (-4.f), 0.5f, 1e-6f);
}

// The order the two do their work in is audible, so it is nailed down here.
// Squeezed first, then turned: the ellipse belongs to the figure and travels
// with it. The other way round the ellipse would stand still in the room, and
// a spinning circle -- the commonest thing anyone records -- would look as
// though the spin had stopped working.
TEST (TrajectoryShaping, TheSqueezeBelongsToTheFigureAndTurnsWithIt)
{
  PlaneShaping shaping;
  shaping.turns = 0.25f;
  shaping.squeezeY = -1.f;

  auto const p = Pos::fromCartesian (0.4f, 0.8f, 0.f);

  auto const squeezedThenTurned = spinPosition (
      Pos::fromCartesian (p.x (), p.y () * 0.5f, 0.f), shaping.turns);
  auto const shaped = shapedPosition (p, shaping);

  EXPECT_NEAR (shaped.x (), squeezedThenTurned.x (), 1e-5f);
  EXPECT_NEAR (shaped.y (), squeezedThenTurned.y (), 1e-5f);
}

// With both pots at their middle this has to be the spin and nothing else --
// the five places that used to call spinPosition() call this instead, and a
// clip with no squeeze in it must not start playing somewhere new.
TEST (TrajectoryShaping, WithoutASqueezeItIsExactlyTheSpin)
{
  PlaneShaping shaping;
  shaping.turns = 0.3f;

  auto const p = Pos::fromCartesian (-0.6f, 0.2f, 0.f);
  auto const shaped = shapedPosition (p, shaping);
  auto const spun = spinPosition (p, shaping.turns);

  EXPECT_NEAR (shaped.x (), spun.x (), 1e-5f);
  EXPECT_NEAR (shaped.y (), spun.y (), 1e-5f);
}

// The standing angle and the running one are one rotation asked for in two
// ways. Summed here, in the one place the engine and the renderer both read,
// so neither can turn a shape the other does not.
TEST (TrajectoryShaping, ThePatternsTurnIsItsRotatePlusItsSpinPhase)
{
  Pattern pattern;
  pattern.setRotate (0.2f);
  pattern.setSpinPhase (0.05f);
  pattern.setSqueezeX (0.5f);
  pattern.setSqueezeY (-0.25f);

  auto const shaping = shapingOf (pattern);

  EXPECT_NEAR (shaping.turns, 0.25f, 1e-5f);
  EXPECT_NEAR (shaping.squeezeX, 0.5f, 1e-5f);
  EXPECT_NEAR (shaping.squeezeY, -0.25f, 1e-5f);
}
