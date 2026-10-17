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

#include <a3-motion-engine/flight/FigureMath.hh>
#include <a3-motion-engine/flight/SoftFigures.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>
#include <a3-motion-engine/util/SeedSpread.hh>

#include <cmath>

using namespace a3;
using namespace a3::figurefixtures;

// The soft figures: a slow straight pass at a group's escort radius, and
// the escort's circle widened until the room hears it go round.

namespace
{
constexpr int fourFour = 4;
FlightTuning const flight;
GameTuning const tuning;

FlightBodies
groupAt (Vec2 at, float mass = FlightTuning{}.groupMass)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { at, mass, 2 };
  return bodies;
}

GamePlan
softPlan (PilotGame game, FlightBodies const &bodies,
          std::array<bool, flightShips> const &crew = crewOf ({ 0 }), double opening = 8.)
{
  juce::Random dice (spreadSeed (3));
  FigureCall call;
  call.openingBeats = opening;
  return planGame (game, 0, crew, cruising (), PilotTarget{}, bodies, MusicCue{}, opening,
                   fourFour, dice, flight, tuning, call);
}

/** Degrees between two directions seen from the middle. */
float
degreesSeen (Vec2 a, Vec2 b)
{
  return std::abs (std::remainder (angleOf (a) - angleOf (b), 2.f * pi<float> ())) * 180.f
         / pi<float> ();
}

double
tick ()
{
  return 1. / TempoClock::getTicksPerBeat ();
}
}

TEST (GrazeFigure, ItPassesTheGroupAtItsEscortRadiusNeverCloser)
{
  Vec2 const group{ 0.5f, 0.3f };
  auto const plan = softPlan (PilotGame::Graze, groupAt (group));
  ASSERT_GT (plan.endBeats, plan.passFromBeats);
  auto const escort = escortRadius (flight.groupMass, flight);
  auto nearest = 10.f;
  for (auto beats = plan.passFromBeats; beats <= plan.endBeats; beats += tick ())
    nearest = std::min (nearest,
                        figureGoal (plan, 0, beats, fourFour, tuning).at.getDistanceFrom (group));
  EXPECT_NEAR (nearest, escort, 2e-3f) << "as close as an escort flies";
  EXPECT_GE (nearest, escort - 1e-4f) << "and never closer: a pass, not a park";
}

TEST (GrazeFigure, ItSweepsAtLeastAHeardBendSeenFromTheMiddle)
{
  auto const plan = softPlan (PilotGame::Graze, groupAt ({ 0.5f, 0.3f }));
  EXPECT_GE (degreesSeen (plan.run[0], plan.run[1]), tuning.grazeMinSweepDegrees);
}

TEST (GrazeFigure, ItStaysOffTheMiddle)
{
  for (auto const group : { Vec2{ 0.5f, 0.3f }, Vec2{ -0.3f, 0.2f }, Vec2{ 0.1f, -0.7f } })
    {
      auto const plan = softPlan (PilotGame::Graze, groupAt (group));
      for (auto beats = plan.startBeats; beats <= plan.endBeats; beats += tick ())
        EXPECT_GE (figureGoal (plan, 0, beats, fourFour, tuning).at.getDistanceFromOrigin (),
                   tuning.grazeInnerRadius - 1e-4f)
            << beats;
    }
}

TEST (GrazeFigure, ItPassesAtCruisePace)
{
  auto const plan = softPlan (PilotGame::Graze, groupAt ({ 0.5f, 0.3f }));
  auto const midway = 0.5 * (plan.passFromBeats + plan.endBeats);
  auto const speed
      = figureGoal (plan, 0, midway, fourFour, tuning).velocity.getDistanceFromOrigin ();
  EXPECT_NEAR (speed, cruisePace (fourFour, flight) * tuning.grazePaceFactor, 1e-4f);
}

TEST (GrazeFigure, AGroupAtTheMiddleOrTooHeavyToPassLeavesNoRun)
{
  EXPECT_FALSE (grazeRun ({ 0.f, 0.f }, flight.groupMass, flight, tuning).has_value ());
  EXPECT_FALSE (grazeRun ({ 0.5f, 0.f }, flight.hotspotMass, flight, tuning).has_value ());
  EXPECT_TRUE (grazeRun ({ 0.5f, 0.3f }, flight.groupMass, flight, tuning).has_value ());
}

TEST (GrazeFigure, AGroupAtTheMiddleLeavesNoRun)
{
  auto const plan = softPlan (PilotGame::Graze, groupAt ({ 0.f, 0.f }));
  EXPECT_DOUBLE_EQ (plan.endBeats, plan.startBeats) << "over as it opens";
}

TEST (GrazeFigure, ItBreathesAndIsOverWithinItsBars)
{
  auto const plan = softPlan (PilotGame::Graze, groupAt ({ 0.5f, 0.3f }));
  EXPECT_TRUE (figureStance (plan, 0, plan.passFromBeats, fourFour, tuning).breathes);
  EXPECT_FALSE (figureStance (plan, 0, plan.passFromBeats, fourFour, tuning).hold);
  EXPECT_LE (plan.endBeats - plan.startBeats, tuning.grazeMaxBars * fourFour + 1e-9);
}

TEST (EncircleFigure, ItSubtendsAHeardBendForGroupsNearAndFar)
{
  for (auto const d : { 0.2f, 0.4f, 0.8f, 1.2f })
    {
      auto const radius = encircleRadius ({ d, 0.f }, flight.groupMass, flight, tuning);
      auto const subtended = radius >= d ? 360.f
                                         : 2.f * std::asin (radius / d) * 180.f / pi<float> ();
      EXPECT_GE (subtended, tuning.encircleMinDegrees - 1e-3f) << d;
      EXPECT_GE (radius, escortRadius (flight.groupMass, flight))
          << "never tighter than an escort";
    }
}

TEST (EncircleFigure, ItCirclesForItsBarsAtTheEscortsLap)
{
  Vec2 const group{ -0.4f, 0.3f };
  auto const plan = softPlan (PilotGame::Encircle, groupAt (group));
  EXPECT_DOUBLE_EQ (plan.endBeats, 8. + tuning.encircleBars * fourFour);
  auto const lap = static_cast<double> (flight.captureLapBars) * fourFour;
  auto const a = figureGoal (plan, 0, 8., fourFour, tuning).at - group;
  auto const b = figureGoal (plan, 0, 8. + lap * 0.25, fourFour, tuning).at - group;
  EXPECT_NEAR (degreesSeen (a, b), 90.f, 0.1f) << "a quarter lap in a quarter of its lap";
  EXPECT_NEAR (a.getDistanceFromOrigin (), plan.radius, 1e-5f);
  EXPECT_TRUE (figureStance (plan, 0, 9., fourFour, tuning).breathes);
}

TEST (EncircleFigure, APartnerCirclesOpposite)
{
  Vec2 const group{ -0.4f, 0.3f };
  auto const plan = softPlan (PilotGame::Encircle, groupAt (group), crewOf ({ 0, 1 }));
  for (auto const beats : { 8., 9.5, 13. })
    {
      auto const a = figureGoal (plan, 0, beats, fourFour, tuning).at;
      auto const b = figureGoal (plan, 1, beats, fourFour, tuning).at;
      EXPECT_NEAR (((a + b) * 0.5f).getDistanceFrom (group), 0.f, 1e-4f) << beats;
    }
}
