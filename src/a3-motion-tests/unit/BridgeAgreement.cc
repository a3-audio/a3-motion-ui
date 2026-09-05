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

#include <JuceHeader.h>

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TrajectoryBridges.hh>
#include <a3-motion-engine/TrajectoryShape.hh>

#include <algorithm>

using namespace a3;

namespace
{
/** Three runs with wide gaps between them -- enough small steps that the jump
 *  threshold, which is eight times the median step, sees the gaps as gaps. */
std::vector<Pos>
threeRuns ()
{
  std::vector<Pos> ticks;
  auto run = [&ticks] (float x, float y) {
    for (int i = 0; i < 10; ++i)
      ticks.push_back (
          Pos::fromCartesian (x + 0.02f * static_cast<float> (i), y, 0.f));
  };
  run (-0.7f, 0.0f);
  run (0.4f, 0.35f);
  run (-0.2f, -0.4f);
  return ticks;
}

std::shared_ptr<Pattern>
aPatternOf (std::vector<Pos> const &ticks)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->resize (ticks.size ());
  for (index_t tick = 0; tick < ticks.size (); ++tick)
    pattern->setTick (tick, ticks[tick]);
  pattern->markComplete ();
  return pattern;
}
}

// The one failure this whole design exists to prevent: the drawn line and the
// played movement disagreeing about where a gap is, so the sphere shows a line
// the blob does not run on. They have to break in the same places at every
// setting of the reach, not only at the default.
TEST (BridgeAgreement, TheLineBreaksWhereTheMovementJumps)
{
  auto const ticks = threeRuns ();
  auto const pattern = aPatternOf (ticks);

  for (int step = 0; step <= 10; ++step)
    {
      auto const reach = static_cast<float> (step) / 10.f;
      pattern->setFadeReach (reach);
      auto const plan = pattern->getBridgePlan ();

      // Where the drawn line is cut: the last tick of every segment but the
      // last one.
      auto const segments = trajectorySegments (ticks, plan);
      size_t drawn = 0;
      std::vector<size_t> cuts;
      for (size_t s = 0; s + 1 < segments.size (); ++s)
        {
          drawn += segments[s].size ();
          cuts.push_back (drawn - 1);
        }

      for (auto const cut : cuts)
        EXPECT_FALSE (plan.bridged (static_cast<index_t> (cut)))
            << "the line is cut at tick " << cut
            << " but the movement draws through it, at reach " << reach;

      for (auto const &bridge : plan.bridges)
        {
          // The wrapping step is never a drawn edge either way.
          if (bridge.fromTick + 1 >= ticks.size ())
            continue;
          EXPECT_EQ (std::find (cuts.begin (), cuts.end (),
                                static_cast<size_t> (bridge.fromTick)),
                     cuts.end ())
              << "the movement draws through tick " << bridge.fromTick
              << " but the line is cut there, at reach " << reach;
        }
    }
}

namespace
{
/** Two taps, each held for half the take.
 *
 *  Held for many ticks rather than one, because the jump threshold is eight
 *  times the MEDIAN step: with only four ticks the median IS the jump, the
 *  threshold lands at 12.8, and nothing is a jump at all. A real tapped take
 *  stands still between its taps, and a fixture that does not looks nothing
 *  like one. */
constexpr index_t tapTicks = 32;
constexpr double halfWayAcross = tapTicks - 0.5;

std::shared_ptr<Pattern>
twoTapsHeld ()
{
  std::vector<Pos> ticks;
  for (index_t i = 0; i < tapTicks; ++i)
    ticks.push_back (Pos::fromCartesian (-0.8f, 0.f, 0.f));
  for (index_t i = 0; i < tapTicks; ++i)
    ticks.push_back (Pos::fromCartesian (0.8f, 0.f, 0.f));
  return aPatternOf (ticks);
}
}

// The behaviour the jump-hold was added for in the first place, which has to
// survive the change: at no reach at all, a tapped take stands at its taps.
TEST (BridgeAgreement, AtZeroReachATappedTakeStillStandsAtItsTaps)
{
  auto const pattern = twoTapsHeld ();
  pattern->setFadeReach (0.f);

  // Half way across the jump: held on the tap it left, not half way there.
  EXPECT_FLOAT_EQ (pattern->getInterpolatedTick (halfWayAcross).x (), -0.8f);
}

// And with the reach opened up, the same take is drawn through instead. This
// pair is the whole feature: one take, one knob, two behaviours.
TEST (BridgeAgreement, AtFullReachTheSameTakeIsDrawnThrough)
{
  auto const pattern = twoTapsHeld ();
  pattern->setFadeReach (1.f);

  EXPECT_NEAR (pattern->getInterpolatedTick (halfWayAcross).x (), 0.f, 1e-5);
}
