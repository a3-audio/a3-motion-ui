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

#include <a3-motion-engine/BlobInertia.hh>

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

namespace a3
{

namespace
{
Pos
at (float x, float y)
{
  return Pos::fromCartesian (x, y, 0.f);
}

float
distance (Pos const &a, Pos const &b)
{
  return std::hypot (a.x () - b.x (), a.y () - b.y ());
}

/** Run the spring for a number of bars and hand back where it ended up. */
InertialState settle (Pos target, float elasticity, float ticksPerBar,
                      float bars, InertialState state = {})
{
  auto const ticks = static_cast<int> (ticksPerBar * bars);
  for (int tick = 0; tick < ticks; ++tick)
    state = stepInertia (state, target, elasticity, ticksPerBar);
  return state;
}
}

TEST (BlobInertia, RigidIsTheTargetExactlyAndNothingElse)
{
  // Zero has to be *exactly* what the engine does today, to the bit. Every
  // take and every set out there has no value for this and gets the zero, and
  // they have to sound like they did yesterday. The same promise slewTowards()
  // and envelopeOver() already make.
  InertialState state;
  auto const target = at (0.37f, -0.12f);

  for (int tick = 0; tick < 64; ++tick)
    {
      state = stepInertia (state, target, 0.f, 384.f);
      EXPECT_FLOAT_EQ (state.position.x (), target.x ());
      EXPECT_FLOAT_EQ (state.position.y (), target.y ());
      EXPECT_FLOAT_EQ (state.velocity.x (), 0.f);
      EXPECT_FLOAT_EQ (state.velocity.y (), 0.f);
    }
}

TEST (BlobInertia, TheFirstTickLandsOnTheTargetRatherThanFlyingAtIt)
{
  // A cold state has no position. Started at the origin it would be dragged
  // across the sphere to wherever the clip begins, and that travel would go
  // out on OSC.
  InertialState state;
  auto const start = at (-0.6f, 0.25f);

  state = stepInertia (state, start, 0.8f, 384.f);

  EXPECT_FLOAT_EQ (state.position.x (), start.x ());
  EXPECT_FLOAT_EQ (state.position.y (), start.y ());
  EXPECT_TRUE (state.primed);
}

TEST (BlobInertia, ASoftSpringLagsBehindAMovingTarget)
{
  // The whole point: the sound does not arrive where the figure says, it
  // arrives a little later.
  auto constexpr ticksPerBar = 384.f;
  InertialState soft;
  InertialState rigid;

  // Walk the target steadily across, in steps small enough not to be a jump.
  for (int tick = 0; tick < 200; ++tick)
    {
      auto const target = at (static_cast<float> (tick) * 0.002f, 0.f);
      soft = stepInertia (soft, target, 0.8f, ticksPerBar);
      rigid = stepInertia (rigid, target, 0.f, ticksPerBar);
    }

  EXPECT_LT (soft.position.x (), rigid.position.x ());
  EXPECT_GT (rigid.position.x () - soft.position.x (), 0.01f);
}

TEST (BlobInertia, ItOvershootsAStepAndThenComesToRestOnIt)
{
  auto constexpr ticksPerBar = 384.f;

  // Sitting still at the origin, then the target steps sideways -- but not so
  // far that it counts as a jump.
  auto state = settle (at (0.f, 0.f), 0.7f, ticksPerBar, 2.f);
  auto const target = at (inertiaCutDistance * 0.5f, 0.f);

  auto furthest = 0.f;
  for (int tick = 0; tick < static_cast<int> (ticksPerBar * 4.f); ++tick)
    {
      state = stepInertia (state, target, 0.7f, ticksPerBar);
      furthest = std::max (furthest, state.position.x ());
    }

  EXPECT_GT (furthest, target.x ()) << "a spring with this little damping "
                                       "has to swing past";
  EXPECT_LT (distance (state.position, target), 1e-4f)
      << "and it has to come to rest on the target, not near it";
}

TEST (BlobInertia, AJumpLaysTheSpringDownAfreshInsteadOfDraggingIt)
{
  // A clip looping back to its start, a Random end action, stop then play.
  // Dragged, the sound travels a path nobody played -- and that travel goes
  // out on OSC.
  auto constexpr ticksPerBar = 384.f;
  auto state = settle (at (-0.5f, 0.f), 0.8f, ticksPerBar, 2.f);

  auto const elsewhere = at (0.7f, 0.3f);
  state = stepInertia (state, elsewhere, 0.8f, ticksPerBar);

  EXPECT_FLOAT_EQ (state.position.x (), elsewhere.x ());
  EXPECT_FLOAT_EQ (state.position.y (), elsewhere.y ());
  EXPECT_FLOAT_EQ (state.velocity.x (), 0.f);
  EXPECT_FLOAT_EQ (state.velocity.y (), 0.f);
}

TEST (BlobInertia, TheSameBarsGiveTheSameTravelAtAnyTempo)
{
  // This is the test the decision rests on: the spring counts in bars, not in
  // seconds, so a take feels the same at 90 and at 140 BPM. The tick rate is
  // what changes with tempo, and it must not change the shape of the answer.
  auto const walk = [] (float ticksPerBar) {
    InertialState state;
    auto const ticks = static_cast<int> (ticksPerBar);
    for (int tick = 0; tick <= ticks; ++tick)
      {
        // One bar, the target crossing a fixed distance in that bar.
        auto const share = static_cast<float> (tick) / ticksPerBar;
        state = stepInertia (state, at (share * 0.4f, 0.f), 0.75f, ticksPerBar);
      }
    return state.position.x ();
  };

  auto const coarse = walk (192.f);
  auto const fine = walk (768.f);

  EXPECT_NEAR (coarse, fine, 0.004f)
      << "coarse " << coarse << " vs fine " << fine;
}

TEST (BlobInertia, TheStiffestSettingStaysStable)
{
  // A spring integrated per tick blows up once omega passes about a radian a
  // tick. The stiffest end has to stay well under that at the coarsest tick
  // rate the clock runs at.
  auto constexpr coarse = 96.f; // slow tempo, fewest ticks in a bar
  InertialState state;

  for (int tick = 0; tick < 4000; ++tick)
    {
      auto const target = at (std::sin (static_cast<float> (tick) * 0.05f) * 0.3f,
                              0.f);
      state = stepInertia (state, target, 0.02f, coarse);
      ASSERT_TRUE (std::isfinite (state.position.x ()));
      ASSERT_LT (std::abs (state.position.x ()), 2.f) << "at tick " << tick;
    }
}

TEST (BlobInertia, MoreElasticityLagsFurther)
{
  auto constexpr ticksPerBar = 384.f;

  auto const lagOf = [] (float elasticity) {
    InertialState state;
    for (int tick = 0; tick < 300; ++tick)
      state = stepInertia (state, at (static_cast<float> (tick) * 0.001f, 0.f),
                           elasticity, ticksPerBar);
    return 0.299f - state.position.x ();
  };

  EXPECT_GT (lagOf (0.9f), lagOf (0.5f));
  EXPECT_GT (lagOf (0.5f), lagOf (0.2f));
  EXPECT_GT (lagOf (0.2f), 0.f);
}

}
