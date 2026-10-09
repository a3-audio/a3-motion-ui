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
#include <a3-motion-engine/util/Geometry.hh>

#include <algorithm>
#include <cmath>

using namespace a3;

// The four games as figures: where each ship of a game steers for, beat by
// beat. Seen from the middle of the room, where the bends are heard.

namespace
{
constexpr int fourFour = 4;
FlightTuning const flight;
GameTuning const tuning;

float
degreesOf (Vec2 p)
{
  return std::atan2 (p.y, p.x) * 180.f / pi<float> ();
}

/** How far apart two floor points are heard from the middle, 0..180. */
float
degreesBetween (Vec2 a, Vec2 b)
{
  return std::abs (std::remainder (degreesOf (a) - degreesOf (b), 360.f));
}

std::array<Vec2, flightShips> const spread{ Vec2{ 0.7f, 0.f }, Vec2{ 0.f, 0.7f },
                                            Vec2{ -0.7f, 0.f }, Vec2{ 0.f, -0.7f } };

/** The four ships on `spread`, each flying counter-clockwise. */
std::array<ShipState, flightShips>
shipsOnTheSpread ()
{
  std::array<ShipState, flightShips> ships{};
  for (size_t i = 0; i < ships.size (); ++i)
    ships[i] = { spread[i], Vec2{ -spread[i].y, spread[i].x } * 0.5f };
  return ships;
}

FlightBodies
oneGroupAt (Vec2 at, int id = 2)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { at, flight.groupMass, id };
  return bodies;
}

std::array<bool, flightShips>
only (int ship)
{
  std::array<bool, flightShips> crew{};
  crew[static_cast<size_t> (ship)] = true;
  return crew;
}

std::array<bool, flightShips> const everyone{ true, true, true, true };

MusicCue
heading (MusicSection now, MusicSection next, long long changeBar)
{
  MusicCue cue;
  cue.section = now;
  cue.next = next;
  cue.changeBar = changeBar;
  cue.energy = 0.5f;
  return cue;
}

GamePlan
planned (PilotGame game, std::array<bool, flightShips> const &crew, FlightBodies const &bodies,
         MusicCue const &cue = heading (MusicSection::Build, MusicSection::Drop, 4),
         int beatsPerBar = fourFour)
{
  juce::Random dice (5);
  return planGame (game, 0, crew, shipsOnTheSpread (), PilotTarget{}, bodies, cue, 0.,
                   beatsPerBar, dice, flight, tuning);
}

MusicCue
breakdownEndingAtBar (long long bar)
{
  return heading (MusicSection::Breakdown, MusicSection::Build, bar);
}

bool
finite (OrbitPoint const &goal)
{
  return std::isfinite (goal.at.x) && std::isfinite (goal.at.y) && std::isfinite (goal.velocity.x)
         && std::isfinite (goal.velocity.y);
}
}

TEST (GameFigures, TheFakeOutHeadsForItsTargetVeersOffAndStrikesOnTheOne)
{
  auto const bodies = oneGroupAt ({ -0.5f, 0.3f });
  auto const plan = planned (PilotGame::FakeOut, only (0), bodies);
  ASSERT_DOUBLE_EQ (plan.climaxBeats, 16.);
  EXPECT_DOUBLE_EQ (plan.endBeats, 20.);
  auto const target = bodies.body[0].at;

  // Through the build the goal creeps towards the group and ends short of it.
  auto const early = figureGoal (plan, 0, 1., fourFour, tuning);
  auto const late = figureGoal (plan, 0, 11.9, fourFour, tuning);
  EXPECT_GT (early.at.getDistanceFrom (target), late.at.getDistanceFrom (target));
  EXPECT_NEAR (late.at.getDistanceFrom (target), tuning.approachStandOff, 0.05f);
  EXPECT_GT (early.velocity.getDistanceFromOrigin (), 0.f) << "a goal that moves";

  // A bar before the 1 it turns away: a bend the room hears.
  auto const veer = figureGoal (plan, 0, 12.5, fourFour, tuning);
  EXPECT_GE (degreesBetween (veer.at, target), tuning.heardBendDegrees);
  EXPECT_NEAR (degreesBetween (veer.at, target), tuning.veerDegrees, 0.01f);

  // From strikeBeats before the 1 it goes for the group itself.
  EXPECT_EQ (figureGoal (plan, 0, 16. - tuning.strikeBeats, fourFour, tuning).at, target);
  EXPECT_EQ (figureGoal (plan, 0, 17., fourFour, tuning).at, target);
}

TEST (GameFigures, TheVeerBeginsOnTheBarLineAndTheStrikeOnItsBeat)
{
  auto const bodies = oneGroupAt ({ -0.5f, 0.3f });
  auto const plan = planned (PilotGame::FakeOut, only (0), bodies);
  auto const target = bodies.body[0].at;
  auto const veerFrom = plan.climaxBeats - fourFour;
  auto const strikeFrom = plan.climaxBeats - tuning.strikeBeats;

  // The last instant of the approach still stands just short of the group.
  auto const approaching = figureGoal (plan, 0, veerFrom - 1e-3, fourFour, tuning).at;
  EXPECT_NEAR (approaching.getDistanceFrom (target), tuning.approachStandOff, 1e-3f);
  // On the bar line it is off to the side, and stays there to the last instant.
  EXPECT_NEAR (degreesBetween (figureGoal (plan, 0, veerFrom, fourFour, tuning).at, target),
               tuning.veerDegrees, 0.01f);
  EXPECT_NEAR (
      degreesBetween (figureGoal (plan, 0, strikeFrom - 1e-3, fourFour, tuning).at, target),
      tuning.veerDegrees, 0.01f);
  EXPECT_EQ (figureGoal (plan, 0, strikeFrom, fourFour, tuning).at, target);
  // Past the game's end it still holds the target: no figure jumps after its 1.
  EXPECT_EQ (figureGoal (plan, 0, plan.endBeats + 1., fourFour, tuning).at, target);
}

TEST (GameFigures, TheVeerNeverHugsTheMiddle)
{
  auto const plan = planned (PilotGame::FakeOut, only (0), oneGroupAt ({ 0.1f, 0.f }));
  auto const veer = figureGoal (plan, 0, 12.5, fourFour, tuning);
  EXPECT_GE (veer.at.getDistanceFromOrigin (), tuning.veerMinRadius - 1e-5f);
}

TEST (GameFigures, TheVeerKeepsTheTargetsRadiusFromTheMinimumOut)
{
  auto const onTheLine
      = planned (PilotGame::FakeOut, only (0), oneGroupAt ({ -tuning.veerMinRadius, 0.f }));
  EXPECT_NEAR (figureGoal (onTheLine, 0, 12.5, fourFour, tuning).at.getDistanceFromOrigin (),
               tuning.veerMinRadius, 1e-5f);

  auto const further = planned (PilotGame::FakeOut, only (0), oneGroupAt ({ -0.6f, 0.f }));
  EXPECT_NEAR (figureGoal (further, 0, 12.5, fourFour, tuning).at.getDistanceFromOrigin (), 0.6f,
               1e-5f);
}

TEST (GameFigures, WithNoGroupTheFakeOutAimsAcrossTheRoom)
{
  auto const plan = planned (PilotGame::FakeOut, only (0), FlightBodies{});
  EXPECT_EQ (plan.targetBodyId, noBodyId);
  EXPECT_NEAR (plan.target.x, -tuning.lonelyTargetRadius, 1e-5f);
  EXPECT_NEAR (plan.target.y, 0.f, 1e-5f);
}

TEST (GameFigures, TheFakeOutFollowsItsTargetWhereverItIsDragged)
{
  auto plan = planned (PilotGame::FakeOut, only (0), oneGroupAt ({ -0.5f, 0.3f }));
  Vec2 const dragged{ 0.f, -0.6f };
  plan.target = dragged;
  EXPECT_NEAR (degreesBetween (figureGoal (plan, 0, 12.5, fourFour, tuning).at, dragged),
               tuning.veerDegrees, 0.01f);
  EXPECT_EQ (figureGoal (plan, 0, 15., fourFour, tuning).at, dragged);
}

TEST (GameFigures, AShipStartingOnItsTargetStillGetsAFiniteFigure)
{
  auto const bodies = oneGroupAt ({ 0.7f, 0.f });
  juce::Random dice (5);
  auto const plan = planGame (PilotGame::FakeOut, 0, only (0), shipsOnTheSpread (), PilotTarget{},
                              bodies, heading (MusicSection::Build, MusicSection::Drop, 4), 0.,
                              fourFour, dice, flight, tuning);
  for (auto beats : { 0., 1., 11.9, 12.5, 15., 17. })
    EXPECT_TRUE (finite (figureGoal (plan, 0, beats, fourFour, tuning))) << beats;
}

TEST (GameFigures, ACrewsFakeOutsVeerToBothSides)
{
  auto const plan = planned (PilotGame::FakeOut, everyone, oneGroupAt ({ -0.5f, 0.3f }));
  auto const left = std::count (plan.turn.begin (), plan.turn.end (), 1.f);
  auto const right = std::count (plan.turn.begin (), plan.turn.end (), -1.f);
  EXPECT_EQ (left, 2);
  EXPECT_EQ (right, 2);
}

TEST (GameFigures, TheFormationStandsInALineAcrossItsAxis)
{
  auto const plan = planned (PilotGame::Formation, everyone, oneGroupAt ({ 0.f, 0.8f }));
  EXPECT_NEAR (plan.axis, pi<float> () / 2.f, 1e-5f);
  std::array<float, flightShips> across{};
  for (auto s = 0; s < flightShips; ++s)
    {
      auto const slot = figureGoal (plan, s, 8., fourFour, tuning).at;
      EXPECT_NEAR (slot.y, tuning.formationDistance, 1e-5f) << s;
      across[static_cast<size_t> (s)] = slot.x;
    }
  std::sort (across.begin (), across.end ());
  for (size_t i = 1; i < across.size (); ++i)
    EXPECT_NEAR (across[i] - across[i - 1], tuning.formationSpacing, 1e-5f);
}

TEST (GameFigures, WithNoGroupTheFormationFacesItsLeadersSide)
{
  auto const plan = planned (PilotGame::Formation, everyone, FlightBodies{});
  EXPECT_NEAR (plan.axis, 0.f, 1e-5f) << "the leader stands at 0 deg";
  for (auto s = 0; s < flightShips; ++s)
    EXPECT_NEAR (figureGoal (plan, s, 8., fourFour, tuning).at.x, tuning.formationDistance, 1e-5f)
        << s;
}

TEST (GameFigures, NoShipCrossesAnotherToTakeItsPlace)
{
  // Facing up the floor the line runs from right to left; a ship takes the
  // place on its own side, the lower channel first on a tie.
  auto const plan = planned (PilotGame::Formation, everyone, oneGroupAt ({ 0.f, 0.8f }));
  EXPECT_EQ (plan.part[0], 0);
  EXPECT_EQ (plan.part[1], 1);
  EXPECT_EQ (plan.part[3], 2);
  EXPECT_EQ (plan.part[2], 3);
  EXPECT_GT (figureGoal (plan, 0, 8., fourFour, tuning).at.x, 0.f);
  EXPECT_LT (figureGoal (plan, 2, 8., fourFour, tuning).at.x, 0.f);
}

TEST (GameFigures, EveryShipBurstsAHeardBendFromItsPlaceOnTheOne)
{
  for (auto n = 1; n <= flightShips; ++n)
    {
      std::array<bool, flightShips> crew{};
      for (auto s = 0; s < n; ++s)
        crew[static_cast<size_t> (s)] = true;
      auto const plan = planned (PilotGame::Formation, crew, oneGroupAt ({ 0.f, 0.8f }));
      for (auto s = 0; s < n; ++s)
        {
          auto const slot = figureGoal (plan, s, plan.climaxBeats - 0.1, fourFour, tuning).at;
          auto const burst = figureGoal (plan, s, plan.climaxBeats, fourFour, tuning).at;
          EXPECT_GE (degreesBetween (slot, burst), 45.f) << n << " ships, ship " << s;
          EXPECT_NEAR (burst.getDistanceFromOrigin (), tuning.burstRadius, 1e-5f);
        }
    }
}

TEST (GameFigures, TheBurstTableIsMirroredAndNeverReadPastItsEdge)
{
  // Mirrored in the axis: the two ends burst the same angle to either side
  // (180 deg and -180 deg being the same way).
  for (auto n = 1; n <= flightShips; ++n)
    for (auto place = 0; place < n; ++place)
      EXPECT_NEAR (std::remainder (burstAngle (n, place) + burstAngle (n, n - 1 - place),
                                   2.f * pi<float> ()),
                   0.f, 1e-6f)
          << n << " ships, place " << place;
  EXPECT_FLOAT_EQ (burstAngle (0, 0), burstAngle (1, 0));
  EXPECT_FLOAT_EQ (burstAngle (flightShips + 3, 0), burstAngle (flightShips, 0));
  EXPECT_FLOAT_EQ (burstAngle (flightShips, flightShips + 5),
                   burstAngle (flightShips, flightShips - 1));
  EXPECT_FLOAT_EQ (burstAngle (2, -1), burstAngle (2, 0));
}

TEST (GameFigures, TheHiderSlipsAwayToOneSideAndRunsAcrossBeforeTheOne)
{
  auto const plan = planned (PilotGame::HideAndSeek, only (0), FlightBodies{},
                             heading (MusicSection::Breakdown, MusicSection::Build, 4));
  ASSERT_DOUBLE_EQ (plan.climaxBeats, 16.);
  auto const slipping = figureGoal (plan, 0, 1., fourFour, tuning);
  EXPECT_GT (slipping.velocity.getDistanceFromOrigin (), 0.f) << "it creeps, it does not jump";

  // The ship flies counter-clockwise from 0 deg: it slips round that way.
  auto const hidden = figureGoal (plan, 0, 10., fourFour, tuning).at;
  EXPECT_NEAR (degreesOf (hidden), tuning.hideSideDegrees, 0.01f);
  EXPECT_NEAR (hidden.getDistanceFromOrigin (), tuning.hideRadius, 1e-5f);

  auto const seeking = figureGoal (plan, 0, 16. - tuning.crossBeats, fourFour, tuning).at;
  EXPECT_NEAR (degreesBetween (seeking, hidden), 180.f, 0.01f);
}

TEST (GameFigures, TheHiderSlipsTheWayItFlies)
{
  auto ships = shipsOnTheSpread ();
  ships[0].v = -ships[0].v; // clockwise
  ships[1].v = {};          // standing still: counter-clockwise, the orbit's way
  juce::Random dice (5);
  auto const plan
      = planGame (PilotGame::HideAndSeek, 0, { true, true, false, false }, ships, PilotTarget{},
                  FlightBodies{}, breakdownEndingAtBar (4), 0., fourFour, dice, flight, tuning);
  EXPECT_NEAR (degreesOf (figureGoal (plan, 0, 10., fourFour, tuning).at), -tuning.hideSideDegrees,
               0.01f);
  EXPECT_NEAR (degreesOf (figureGoal (plan, 1, 10., fourFour, tuning).at),
               90.f + tuning.hideSideDegrees, 0.01f);
}

TEST (GameFigures, TheSlipEndsWhereItArrivesAndTheRunStartsOnItsBeat)
{
  auto const plan
      = planned (PilotGame::HideAndSeek, only (0), FlightBodies{}, breakdownEndingAtBar (4));
  auto const slipEnd = tuning.slipBars * fourFour;
  auto const hidden = figureGoal (plan, 0, slipEnd, fourFour, tuning);
  EXPECT_GT (figureGoal (plan, 0, slipEnd - 1e-3, fourFour, tuning).velocity.getDistanceFromOrigin (),
             0.f);
  EXPECT_EQ (hidden.velocity, (Vec2{}));
  EXPECT_NEAR (hidden.at.getDistanceFromOrigin (), tuning.hideRadius, 1e-5f);

  auto const crossFrom = plan.climaxBeats - tuning.crossBeats;
  EXPECT_EQ (figureGoal (plan, 0, crossFrom - 1e-3, fourFour, tuning).at, hidden.at);
  EXPECT_NEAR (degreesBetween (figureGoal (plan, 0, crossFrom, fourFour, tuning).at, hidden.at),
               180.f, 0.01f);
}

TEST (GameFigures, ASlowSlipNeverOutlastsTheRunAcross)
{
  GameTuning slow;
  slow.slipBars = 4.f; // 16 beats: longer than the 13 to the run
  juce::Random dice (5);
  auto const plan = planGame (PilotGame::HideAndSeek, 0, only (0), shipsOnTheSpread (),
                              PilotTarget{}, FlightBodies{}, breakdownEndingAtBar (4), 0.,
                              fourFour, dice, flight, slow);
  auto const crossFrom = plan.climaxBeats - slow.crossBeats;
  auto const lastSlip = figureGoal (plan, 0, crossFrom - 1e-3, fourFour, slow);
  EXPECT_GT (lastSlip.velocity.getDistanceFromOrigin (), 0.f) << "still creeping";
  EXPECT_NEAR (lastSlip.at.getDistanceFromOrigin (), slow.hideRadius, 1e-2f) << "but arrived";
  EXPECT_NEAR (degreesBetween (figureGoal (plan, 0, crossFrom, fourFour, slow).at, lastSlip.at),
               180.f, 1.f);
}

TEST (GameFigures, CallAndResponseTakesTurnsABarEach)
{
  std::array<bool, flightShips> const pair{ true, false, true, false };
  MusicCue groove;
  groove.energy = 0.5f;
  auto const plan = planned (PilotGame::CallAndResponse, pair, FlightBodies{}, groove);
  ASSERT_DOUBLE_EQ (plan.climaxBeats, 4.);
  EXPECT_DOUBLE_EQ (plan.endBeats, 4. + 2. * tuning.exchanges * fourFour);
  EXPECT_EQ (plan.part[0], 0);
  EXPECT_EQ (plan.part[2], 1);

  auto const turned = [&] (int ship, double from) {
    return degreesBetween (figureGoal (plan, ship, from, fourFour, tuning).at,
                           figureGoal (plan, ship, from + 3.99, fourFour, tuning).at);
  };
  EXPECT_NEAR (turned (0, 4.), tuning.callArcDegrees, 0.5f); // the leader calls
  EXPECT_NEAR (turned (2, 4.), 0.f, 0.01f);                  // the other listens
  EXPECT_NEAR (turned (0, 8.), 0.f, 0.01f);
  EXPECT_NEAR (turned (2, 8.), tuning.callArcDegrees, 0.5f); // and answers
  EXPECT_NEAR (turned (0, 12.), tuning.callArcDegrees, 0.5f); // and back
  EXPECT_GE (tuning.callArcDegrees, tuning.heardBendDegrees);

  // Across the room from each other while they gather.
  EXPECT_NEAR (degreesBetween (figureGoal (plan, 0, 2., fourFour, tuning).at,
                               figureGoal (plan, 2, 2., fourFour, tuning).at),
               180.f, 0.01f);
}

TEST (GameFigures, CallAndResponseNeverJumpsOnABarLine)
{
  std::array<bool, flightShips> const pair{ true, false, true, false };
  MusicCue groove;
  groove.energy = 0.5f;
  auto const plan = planned (PilotGame::CallAndResponse, pair, FlightBodies{}, groove);
  for (auto line = plan.climaxBeats; line <= plan.endBeats; line += fourFour)
    for (auto ship : { 0, 2 })
      {
        auto const before = figureGoal (plan, ship, line - 1e-3, fourFour, tuning).at;
        auto const on = figureGoal (plan, ship, line, fourFour, tuning).at;
        EXPECT_LT (before.getDistanceFrom (on), 0.01f) << "ship " << ship << " at " << line;
      }
  // A call moves at the arc's pace, a listener stands.
  auto const calling = figureGoal (plan, 0, 5., fourFour, tuning).velocity.getDistanceFromOrigin ();
  EXPECT_NEAR (calling, tuning.callRadius * tuning.callArcDegrees * pi<float> () / 180.f / fourFour,
               1e-5f);
  EXPECT_EQ (figureGoal (plan, 2, 5., fourFour, tuning).velocity, (Vec2{}));
}

TEST (GameFigures, CallAndResponseCountsItsBarsInThreeFour)
{
  std::array<bool, flightShips> const pair{ true, true, false, false };
  MusicCue groove;
  groove.energy = 0.5f;
  auto const plan = planned (PilotGame::CallAndResponse, pair, FlightBodies{}, groove, 3);
  ASSERT_DOUBLE_EQ (plan.climaxBeats, 3.);
  EXPECT_DOUBLE_EQ (plan.endBeats, 3. + 2. * tuning.exchanges * 3);
  auto const called = degreesBetween (figureGoal (plan, 0, 3., 3, tuning).at,
                                      figureGoal (plan, 0, 5.99, 3, tuning).at);
  EXPECT_NEAR (called, tuning.callArcDegrees, 0.5f);
}

TEST (GameFigures, TheNearestGroupIsTheTargetAndADeadZoneIsNobody)
{
  FlightBodies bodies;
  bodies.count = 4;
  bodies.body[0] = { { 0.6f, 0.f }, flight.deadZoneMass, 0 };
  bodies.body[1] = { { 0.f, 0.6f }, flight.groupMass, 1 };
  bodies.body[2] = { { -0.6f, 0.f }, flight.crowdMass, 2 };
  bodies.body[3] = { { 0.f, -0.6f }, flight.hotspotMass, 3 };
  Vec2 const from{ 0.5f, 0.3f };

  EXPECT_EQ (targetBody (PilotTarget{}, from, bodies, flight), 1);
  EXPECT_EQ (targetBody (PilotTarget{ PilotTargetKind::Group, 3 }, from, bodies, flight), 3);
  EXPECT_EQ (targetBody (PilotTarget{ PilotTargetKind::Group, 0 }, from, bodies, flight), noBodyId)
      << "G1 is a dead zone";
  EXPECT_EQ (targetBody (PilotTarget{ PilotTargetKind::Group, 6 }, from, bodies, flight), noBodyId);
  EXPECT_EQ (targetBody (PilotTarget{ PilotTargetKind::Crowd, -1 }, from, bodies, flight), 2);
  EXPECT_EQ (targetBody (PilotTarget{ PilotTargetKind::Hotspot, -1 }, from, bodies, flight), 3);
  EXPECT_EQ (targetBody (PilotTarget{}, from, FlightBodies{}, flight), noBodyId);

  ASSERT_TRUE (bodyPlace (bodies, 2).has_value ());
  EXPECT_EQ (*bodyPlace (bodies, 2), (Vec2{ -0.6f, 0.f }));
  EXPECT_FALSE (bodyPlace (bodies, 7).has_value ());
  EXPECT_FALSE (bodyPlace (bodies, noBodyId).has_value ());
}

TEST (GameFigures, TwoGroupsAsNearAsEachOtherGoToTheFirstListed)
{
  FlightBodies bodies;
  bodies.count = 2;
  bodies.body[0] = { { 0.f, 0.6f }, flight.groupMass, 5 };
  bodies.body[1] = { { 0.f, -0.6f }, flight.groupMass, 4 };
  EXPECT_EQ (targetBody (PilotTarget{}, Vec2{ 0.3f, 0.f }, bodies, flight), 5);
  std::swap (bodies.body[0], bodies.body[1]);
  EXPECT_EQ (targetBody (PilotTarget{}, Vec2{ 0.3f, 0.f }, bodies, flight), 4);
}

TEST (GameFigures, ALeaderOutsideTheShipsIsTheFirstShip)
{
  for (auto leader : { -1, flightShips })
    {
      juce::Random dice (5);
      auto const plan = planGame (PilotGame::Formation, leader, everyone, shipsOnTheSpread (),
                                  PilotTarget{}, FlightBodies{},
                                  heading (MusicSection::Build, MusicSection::Drop, 4), 0.,
                                  fourFour, dice, flight, tuning);
      EXPECT_EQ (plan.leader, 0) << leader;
      EXPECT_NEAR (plan.axis, 0.f, 1e-5f) << leader;
    }
}

TEST (GameFigures, AShipOutsideTheCrewHasNoFigure)
{
  auto const plan = planned (PilotGame::FakeOut, only (0), FlightBodies{});
  // Asked anyway, it is told to stay where it was: no jump, no NaN.
  EXPECT_EQ (figureGoal (plan, 1, 3., fourFour, tuning).at, spread[1]);
  EXPECT_EQ (figureGoal (plan, 9, 3., fourFour, tuning).at, (Vec2{}));
}

TEST (GameFigures, NoFigureWithoutAMeterOrAGame)
{
  auto const plan = planned (PilotGame::FakeOut, only (0), FlightBodies{});
  EXPECT_EQ (figureGoal (plan, 0, 12.5, 0, tuning).at, spread[0]);
  EXPECT_EQ (figureGoal (plan, 0, 12.5, -1, tuning).at, spread[0]);

  auto const none = planned (PilotGame::None, only (0), FlightBodies{});
  EXPECT_EQ (figureGoal (none, 0, 12.5, fourFour, tuning).at, spread[0]);
  EXPECT_EQ (figureGoal (none, 0, 12.5, fourFour, tuning).velocity, (Vec2{}));
}
