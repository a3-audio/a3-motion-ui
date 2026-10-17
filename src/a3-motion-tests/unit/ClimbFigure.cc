/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include "FigureFixtures.hh"

#include <a3-motion-engine/flight/ClimbFigure.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <cmath>

using namespace a3;
using namespace a3::figurefixtures;

// The riser: a circle that shrinks towards the middle -- the band's top --
// and turns twice as fast on every 4-bar line counted back from the drop,
// landing on its 1.

namespace
{
constexpr int fourFour = 4;
FlightTuning const flight;
GameTuning const tuning;

MusicCue
towardsADrop (long long dropBar, bool previewed)
{
  MusicCue cue;
  cue.section = MusicSection::Build;
  cue.next = MusicSection::Drop;
  cue.changeBar = dropBar;
  cue.previewed = previewed;
  cue.energy = 0.6f;
  return cue;
}

GamePlan
climbPlan (MusicCue const &cue, double opening = 0., int beatsPerBar = fourFour,
           std::array<bool, flightShips> const &crew = crewOf ({ 0 }))
{
  juce::Random dice (3);
  FigureCall call;
  call.openingBeats = opening;
  return planGame (PilotGame::Climb, 0, crew, cruising (), PilotTarget{}, {}, cue, opening,
                   beatsPerBar, dice, flight, tuning, call);
}

/** Degrees ship `ship`'s goal turns round the middle between beats a and b. */
double
turned (GamePlan const &plan, double a, double b, int beatsPerBar = fourFour, int ship = 0)
{
  auto const dt = 1. / TempoClock::getTicksPerBeat ();
  auto const angle = [&] (double t) {
    auto const p = figureGoal (plan, ship, t, beatsPerBar, tuning).at;
    return std::atan2 (p.y, p.x);
  };
  auto total = 0.;
  auto last = angle (a);
  for (auto t = a + dt; t <= b + 1e-9; t += dt)
    {
      auto const now = angle (t);
      total += std::abs (std::remainder (now - last, 2.f * pi<float> ()));
      last = now;
    }
  return total * 180. / pi<double> ();
}
}

TEST (ClimbFigure, TheLapHalvesOnFourBarLinesCountedBackFromTheOne)
{
  EXPECT_FLOAT_EQ (climbLapBars (16., tuning), tuning.climbStartLapBars);
  EXPECT_FLOAT_EQ (climbLapBars (12., tuning), 4.f);
  EXPECT_FLOAT_EQ (climbLapBars (8., tuning), 2.f);
  EXPECT_FLOAT_EQ (climbLapBars (5., tuning), 2.f);
  EXPECT_FLOAT_EQ (climbLapBars (4., tuning), 1.f);
  EXPECT_FLOAT_EQ (climbLapBars (0.5, tuning), 1.f);
  EXPECT_FLOAT_EQ (climbLapBars (40., tuning), tuning.climbStartLapBars)
      << "never slower than the cruise";
}

TEST (ClimbFigure, NeverFasterThanALapABar)
{
  for (auto bars = 0.05; bars < 20.; bars += 0.05)
    EXPECT_GE (climbLapBars (bars, tuning), tuning.climbFastestLapBars) << bars;
}

TEST (ClimbFigure, ItTurnsTwiceAsFastOnEachLine)
{
  auto const plan = climbPlan (towardsADrop (12, true));
  EXPECT_NEAR (turned (plan, 0., 4.), 90., 1.) << "twelve bars out: a lap in four";
  EXPECT_NEAR (turned (plan, 16., 20.), 180., 1.) << "eight bars out: a lap in two";
  EXPECT_NEAR (turned (plan, 32., 36.), 360., 1.) << "the last four: a lap a bar";
}

TEST (ClimbFigure, ItCountsTheRunningMetersBars)
{
  auto const plan = climbPlan (towardsADrop (12, true), 0., 3);
  EXPECT_DOUBLE_EQ (plan.climaxBeats, 36.);
  EXPECT_NEAR (turned (plan, 0., 3., 3), 90., 1.);
  EXPECT_NEAR (turned (plan, 33., 36., 3), 360., 1.);
}

TEST (ClimbFigure, ItClimbsAllTheWayAndNeverBack)
{
  auto const plan = climbPlan (towardsADrop (12, true));
  EXPECT_NEAR (figureGoal (plan, 0, 0., fourFour, tuning).at.getDistanceFromOrigin (),
               flight.orbitRadius, 1e-5f);
  auto last = 10.f;
  for (auto beats = 0.; beats <= 48.; beats += 0.25)
    {
      auto const radius = figureGoal (plan, 0, beats, fourFour, tuning).at.getDistanceFromOrigin ();
      EXPECT_LE (radius, last + 1e-6f) << beats;
      last = radius;
    }
  EXPECT_NEAR (last, tuning.climbTopRadius, 1e-5f) << "on the 1 it is at the top";
}

TEST (ClimbFigure, ItLandsOnTheDropAndChainsOnlyWhenTheDropWasPreviewed)
{
  auto const previewed = climbLanding (towardsADrop (12, true), 0., fourFour, tuning);
  EXPECT_DOUBLE_EQ (previewed.climaxBeats, 48.);
  EXPECT_TRUE (previewed.chainsIntoSlap);
  auto const heard = climbLanding (towardsADrop (12, false), 0., fourFour, tuning);
  EXPECT_DOUBLE_EQ (heard.climaxBeats, 48.);
  EXPECT_FALSE (heard.chainsIntoSlap) << "a drop the room heard has passed its 1";
  auto const nothing = climbLanding (MusicCue{}, 1., fourFour, tuning);
  EXPECT_DOUBLE_EQ (nothing.climaxBeats, 16.) << "the next phrase line a bar away";
  EXPECT_FALSE (nothing.chainsIntoSlap);
  auto const tooClose = climbLanding (towardsADrop (12, true), 46., fourFour, tuning);
  EXPECT_FALSE (tooClose.chainsIntoSlap) << "less than a bar to the drop";
}

TEST (ClimbFigure, WithoutADropItHoldsAtTheTopThenLetsGo)
{
  auto const plan = climbPlan (MusicCue{});
  EXPECT_DOUBLE_EQ (plan.climaxBeats, 16.);
  EXPECT_DOUBLE_EQ (plan.endBeats, 16. + tuning.climbHoldBars * fourFour);
  EXPECT_NEAR (figureGoal (plan, 0, 20., fourFour, tuning).at.getDistanceFromOrigin (),
               tuning.climbTopRadius, 1e-5f);
  EXPECT_NEAR (turned (plan, 16., 16. + tuning.climbHoldLapBars * fourFour), 360., 1.);
}

TEST (ClimbFigure, ItBreathesAndHoldsNothing)
{
  auto const plan = climbPlan (towardsADrop (12, true));
  auto const stance = figureStance (plan, 0, 10., fourFour, tuning);
  EXPECT_TRUE (stance.breathes);
  EXPECT_FALSE (stance.hold);
}

TEST (ClimbFigure, ACrewClimbsSpreadRoundTheCircle)
{
  auto const plan = climbPlan (towardsADrop (12, true), 0., fourFour, crewOf ({ 0, 2 }));
  for (auto const beats : { 0., 10., 30., 47. })
    {
      auto const a = figureGoal (plan, 0, beats, fourFour, tuning).at;
      auto const b = figureGoal (plan, 2, beats, fourFour, tuning).at;
      EXPECT_NEAR ((a + b).getDistanceFromOrigin (), 0.f, 1e-4f) << beats;
    }
}

TEST (ClimbFigure, ADropOpensOnTheBarTheMusicNamesForIt)
{
  EXPECT_TRUE (dropOpensOn (towardsADrop (12, true), 12)) << "the preview not yet turned over";
  MusicCue drop;
  drop.section = MusicSection::Drop;
  drop.sectionStartBar = 12;
  EXPECT_TRUE (dropOpensOn (drop, 12));
  EXPECT_FALSE (dropOpensOn (drop, 13));
  EXPECT_FALSE (dropOpensOn (towardsADrop (16, true), 12)) << "the change moved away";
}
