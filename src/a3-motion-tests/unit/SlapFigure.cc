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

#include <a3-motion-engine/flight/GameFigures.hh>
#include <a3-motion-engine/flight/SlapFigure.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <cmath>
#include <optional>
#include <set>
#include <vector>

using namespace a3;

// The slap as the floor plays it: two sides fixed in the room, a step from
// one to the other on the grid, held in between. Front is +x, left +y.

namespace
{
constexpr int fourFour = 4;
FlightTuning const flight;

/** A tuning whose band never reaches front-back or up-down. */
GameTuning
leftRightOnly ()
{
  GameTuning tuning;
  tuning.highBandFreq = 2.f;
  return tuning;
}

std::array<bool, flightShips>
only (int ship)
{
  std::array<bool, flightShips> crew{};
  crew[static_cast<size_t> (ship)] = true;
  return crew;
}

/** Four ships on a circle of 0.7 between the side points (on the
 *  diagonals, so no side is blocked), flying counter-clockwise. */
std::array<ShipState, flightShips>
cruising ()
{
  constexpr float d = 0.495f; // 0.7 / sqrt 2
  std::array<Vec2, flightShips> const at{ Vec2{ d, d }, Vec2{ -d, d }, Vec2{ -d, -d },
                                         Vec2{ d, -d } };
  std::array<ShipState, flightShips> ships{};
  for (size_t i = 0; i < ships.size (); ++i)
    ships[i] = { at[i], Vec2{ -at[i].y, at[i].x } * 0.5f };
  return ships;
}

FigureCall
openingAt (double beats, bool snaps)
{
  FigureCall call;
  call.openingBeats = beats;
  call.snaps = snaps;
  return call;
}

GamePlan
slapPlan (FigureCall const &call, GameTuning const &tuning,
          std::array<bool, flightShips> const &crew = only (0),
          std::array<ShipState, flightShips> const &ships = cruising (),
          FlightBodies const &bodies = {}, juce::int64 seed = 5, int beatsPerBar = fourFour)
{
  juce::Random dice (seed);
  return planGame (PilotGame::Slap, 0, crew, ships, PilotTarget{}, bodies, MusicCue{},
                   call.openingBeats.value_or (0.), beatsPerBar, dice, flight, tuning, call);
}

/** The beats on which ship `ship`'s held point changes, tick by tick. */
std::vector<double>
eventsOf (GamePlan const &plan, int ship, GameTuning const &tuning, int beatsPerBar = fourFour)
{
  std::vector<double> events;
  std::optional<Vec2> last;
  auto const dt = 1. / TempoClock::getTicksPerBeat ();
  for (auto beats = plan.startBeats - 1.; beats < plan.endBeats + 1.; beats += dt)
    {
      if (!figureStance (plan, ship, beats, beatsPerBar, tuning).hold)
        {
          last.reset ();
          continue;
        }
      auto const at = figureGoal (plan, ship, beats, beatsPerBar, tuning).at;
      if (!last || *last != at)
        events.push_back (beats);
      last = at;
    }
  return events;
}

/** juce::Random's first draw is near 0 for a small seed: spread the seeds. */
juce::int64
spread (int seed)
{
  return seed * 1000003LL;
}

float
degreesBetween (Vec2 a, Vec2 b)
{
  auto const degrees = [] (Vec2 p) { return std::atan2 (p.y, p.x) * 180.f / pi<float> (); };
  return std::abs (std::remainder (degrees (a) - degrees (b), 360.f));
}
}

TEST (SlapFigure, FromTheOneItSnapsToTheFrontThenLeftRightLeft)
{
  auto const tuning = leftRightOnly ();
  auto const plan = slapPlan (openingAt (8., true), tuning);
  EXPECT_EQ (eventsOf (plan, 0, tuning), (std::vector<double>{ 8., 10., 12., 14. }));
  EXPECT_EQ (figureGoal (plan, 0, 8., fourFour, tuning).at, (Vec2{ tuning.slapSideRadius, 0.f }));
  auto const first = figureGoal (plan, 0, 10., fourFour, tuning).at;
  EXPECT_NEAR (first.x, 0.f, 1e-6f);
  EXPECT_NEAR (std::abs (first.y), tuning.slapSideRadius, 1e-6f);
  EXPECT_EQ (figureGoal (plan, 0, 12., fourFour, tuning).at, -first);
  EXPECT_EQ (figureGoal (plan, 0, 14., fourFour, tuning).at, first);
  EXPECT_DOUBLE_EQ (plan.endBeats, 16.) << "two bars from its opening";
}

TEST (SlapFigure, OffTheOneItOpensOnItsFirstSide)
{
  auto const tuning = leftRightOnly ();
  auto const plan = slapPlan (openingAt (9., false), tuning);
  auto const opened = figureGoal (plan, 0, 9., fourFour, tuning).at;
  EXPECT_NEAR (std::abs (opened.y), tuning.slapSideRadius, 1e-6f) << "a side, not the front";
}

TEST (SlapFigure, ItsEventsFallOnTheOneAndTheThreeNeverOnTheBreath)
{
  auto const tuning = leftRightOnly ();
  for (auto const landing : { 8., 9., 10. })
    {
      auto const events = eventsOf (slapPlan (openingAt (landing, false), tuning), 0, tuning);
      ASSERT_FALSE (events.empty ());
      for (size_t i = 0; i < events.size (); ++i)
        {
          auto const inBar = std::fmod (events[i], static_cast<double> (fourFour));
          EXPECT_NE (inBar, 3.) << "opened on " << landing;
          if (i == 0)
            continue;
          EXPECT_TRUE (inBar == 0. || inBar == 2.) << events[i];
          EXPECT_GE (events[i] - events[i - 1], tuning.eventSpacingBeats) << events[i];
        }
    }
}

TEST (SlapFigure, InThreeFourItKeepsOffTheBreath)
{
  auto const tuning = leftRightOnly ();
  auto const plan = slapPlan (openingAt (6., true), tuning, only (0), cruising (), {}, 5, 3);
  auto const events = eventsOf (plan, 0, tuning, 3);
  ASSERT_GE (events.size (), 2u);
  for (size_t i = 0; i < events.size (); ++i)
    {
      EXPECT_NE (std::fmod (events[i], 3.), 2.) << events[i];
      if (i > 0)
        EXPECT_GE (events[i] - events[i - 1], tuning.eventSpacingBeats);
    }
}

TEST (SlapFigure, InTwoFourItStepsOnEveryOne)
{
  auto const tuning = leftRightOnly ();
  auto const plan = slapPlan (openingAt (4., true), tuning, only (0), cruising (), {}, 5, 2);
  EXPECT_EQ (eventsOf (plan, 0, tuning, 2), (std::vector<double>{ 4., 6. }));
}

TEST (SlapFigure, ADropWindowCutsItShort)
{
  auto const tuning = leftRightOnly ();
  auto call = openingAt (8., true);
  call.endByBeats = 12.;
  auto const plan = slapPlan (call, tuning);
  EXPECT_DOUBLE_EQ (plan.endBeats, 12.);
  EXPECT_EQ (eventsOf (plan, 0, tuning), (std::vector<double>{ 8., 10. }));
  EXPECT_FALSE (figureStance (plan, 0, 12., fourFour, tuning).hold);
}

TEST (SlapFigure, TheShipHoldsAndTheEarIsToldHowToCross)
{
  auto const tuning = leftRightOnly ();
  auto const plan = slapPlan (openingAt (8., true), tuning);
  EXPECT_FALSE (figureStance (plan, 0, 7.9, fourFour, tuning).hold) << "not before its beat";
  auto const opened = figureStance (plan, 0, 8., fourFour, tuning);
  EXPECT_TRUE (opened.hold);
  EXPECT_FALSE (opened.breathes);
  EXPECT_EQ (opened.glide, GlidePath::Shortest) << "to the front from wherever it was";
  EXPECT_EQ (figureStance (plan, 0, 10.5, fourFour, tuning).glide, GlidePath::ViaFront);
  EXPECT_FALSE (figureStance (plan, 1, 10.5, fourFour, tuning).hold) << "only its crew";
}

TEST (SlapFigure, FrontBackAndUpDownOnlyWithAHighBand)
{
  GameTuning tuning;
  tuning.highBandFreq = 0.6f;
  std::set<GlidePath> low, high;
  for (auto seed = 0; seed < 60; ++seed)
    {
      auto call = openingAt (8., true);
      call.bandFreq = 0.3f;
      low.insert (slapPlan (call, tuning, only (0), cruising (), {}, spread (seed)).sideGlide);
      call.bandFreq = 0.8f;
      high.insert (slapPlan (call, tuning, only (0), cruising (), {}, spread (seed)).sideGlide);
    }
  EXPECT_EQ (low, (std::set<GlidePath>{ GlidePath::ViaFront }));
  EXPECT_EQ (high, (std::set<GlidePath>{ GlidePath::ViaFront, GlidePath::ViaSide,
                                          GlidePath::Vertical }));
}

TEST (SlapFigure, LeftRightIsThrownMostOften)
{
  GameTuning const tuning;
  std::array<int, 3> thrown{};
  for (auto seed = 0; seed < 600; ++seed)
    {
      auto const glide = slapPlan (openingAt (8., true), tuning, only (0), cruising (), {}, spread (seed))
                             .sideGlide;
      ++thrown[glide == GlidePath::ViaFront ? 0 : (glide == GlidePath::ViaSide ? 1 : 2)];
    }
  EXPECT_GT (thrown[0], thrown[1]);
  EXPECT_GT (thrown[0], thrown[2]);
}

TEST (SlapFigure, TheSidesStandInTheRoomWhereverTheGroupsAre)
{
  GameTuning const tuning;
  FlightBodies here, there;
  here.count = there.count = 1;
  here.body[0] = { { -0.5f, 0.4f }, flight.groupMass, 2 };
  there.body[0] = { { 0.6f, -0.2f }, flight.hotspotMass, 2 };
  for (auto seed = 0; seed < 20; ++seed)
    EXPECT_EQ (slapPlan (openingAt (8., true), tuning, only (0), cruising (), here, spread (seed)).sides,
               slapPlan (openingAt (8., true), tuning, only (0), cruising (), there, spread (seed)).sides)
        << "a side change, not an impulse towards a group";
}

TEST (SlapFigure, ASideNearAnotherShipIsLeftForAnotherAxis)
{
  GameTuning const tuning;
  auto ships = cruising ();
  ships[1].p = { 0.f, 0.8f }; // on the left side point
  for (auto seed = 0; seed < 60; ++seed)
    EXPECT_NE (slapPlan (openingAt (8., true), tuning, only (0), ships, {}, spread (seed)).sideGlide,
               GlidePath::ViaFront)
        << seed;
  auto grounded = openingAt (8., true);
  grounded.flying[1] = false;
  std::set<GlidePath> seen;
  for (auto seed = 0; seed < 60; ++seed)
    {
      juce::Random dice (spread (seed));
      seen.insert (planGame (PilotGame::Slap, 0, only (0), ships, PilotTarget{}, {}, MusicCue{},
                             8., fourFour, dice, flight, tuning, grounded)
                       .sideGlide);
    }
  EXPECT_TRUE (seen.count (GlidePath::ViaFront)) << "a ship that does not fly is not in the way";
}

TEST (SlapFigure, TheSidesAreHeardFarApart)
{
  for (auto const axis : { SlapAxis::LeftRight, SlapAxis::FrontBack })
    {
      auto const sides = slapSides (axis, 0.f, GameTuning{});
      EXPECT_GE (degreesBetween (sides[0], sides[1]), 150.f);
    }
}

TEST (SlapFigure, APartnerSlapsTheOtherWayOnTheSameBeats)
{
  auto const tuning = leftRightOnly ();
  std::array<bool, flightShips> const pair{ true, true, false, false };
  auto const plan = slapPlan (openingAt (8., true), tuning, pair);
  EXPECT_EQ (eventsOf (plan, 0, tuning), eventsOf (plan, 1, tuning));
  for (auto const beat : { 8., 10., 12., 14. })
    EXPECT_EQ (figureGoal (plan, 1, beat, fourFour, tuning).at,
               -figureGoal (plan, 0, beat, fourFour, tuning).at)
        << beat;
}

TEST (SlapFigure, UpDownGoesBetweenTheMiddleAndTheRimAtOneAzimuth)
{
  GameTuning tuning;
  tuning.slapLeftRightWeight = 0.f;
  tuning.slapFrontBackWeight = 0.f;
  auto const plan = slapPlan (openingAt (8., true), tuning);
  EXPECT_EQ (plan.sideGlide, GlidePath::Vertical);
  auto const up = figureGoal (plan, 0, 10., fourFour, tuning).at;
  auto const down = figureGoal (plan, 0, 12., fourFour, tuning).at;
  EXPECT_LT (up.getDistanceFromOrigin (), 0.01f) << "the middle: the band's top";
  EXPECT_NEAR (down.getDistanceFromOrigin (), tuning.slapSideRadius, 1e-5f);
  EXPECT_NEAR (std::atan2 (up.y, up.x), std::atan2 (down.y, down.x), 1e-4f);
}
