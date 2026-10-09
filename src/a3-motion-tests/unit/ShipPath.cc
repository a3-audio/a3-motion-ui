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

#include <cmath>

#include <a3-motion-ui/components/fpv/ShipPath.hh>

using namespace a3;

namespace
{
ShipPathFacts
facts (bool fpv, bool orbit, bool inGame = false, bool escorting = false)
{
  ShipPathFacts f;
  f.fpv = fpv;
  f.orbit = orbit;
  f.inGame = inGame;
  f.escorting = escorting;
  return f;
}
}

TEST (ShipPath, FullDrawsEveryTrajectory)
{
  for (auto orbit : { false, true })
    for (auto game : { false, true })
      for (auto escort : { false, true })
        EXPECT_EQ (shipPathShown (facts (false, orbit, game, escort)),
                   ShipPathShown::Trajectory);
}

TEST (ShipPath, AClipShipKeepsItsTrajectory)
{
  EXPECT_EQ (shipPathShown (facts (true, false)), ShipPathShown::Trajectory);
}

TEST (ShipPath, AnOrbitShipShowsItsOrbit)
{
  EXPECT_EQ (shipPathShown (facts (true, true)), ShipPathShown::Orbit);
}

TEST (ShipPath, AnEscortShowsItsCircle)
{
  EXPECT_EQ (shipPathShown (facts (true, true, false, true)),
             ShipPathShown::Escort);
}

TEST (ShipPath, AShipInAGameShowsNoPath)
{
  // A game steers it to a figure no line here knows: neither its clip nor
  // the orbit is where it flies. The game's own line and word mark it.
  EXPECT_EQ (shipPathShown (facts (true, true, true)), ShipPathShown::None);
  EXPECT_EQ (shipPathShown (facts (true, false, true)), ShipPathShown::None)
      << "a CLIP ship borrowed into a game flies too";
}

TEST (ShipPath, OnlyATrajectoryIsDrawnAsOne)
{
  EXPECT_TRUE (drawsTrajectory (ShipPathShown::Trajectory));
  EXPECT_FALSE (drawsTrajectory (ShipPathShown::Orbit));
  EXPECT_FALSE (drawsTrajectory (ShipPathShown::Escort));
  EXPECT_FALSE (drawsTrajectory (ShipPathShown::None));
}

TEST (ShipPath, AnOrbitIsQuieterThanATrajectory)
{
  EXPECT_FLOAT_EQ (lineLevelOf (ShipPathShown::Trajectory), 1.f);
  EXPECT_LT (lineLevelOf (ShipPathShown::Orbit), 0.7f);
  EXPECT_GT (lineLevelOf (ShipPathShown::Orbit), 0.f);
  EXPECT_FLOAT_EQ (lineLevelOf (ShipPathShown::Escort),
                   lineLevelOf (ShipPathShown::Orbit));
}

TEST (ShipPath, AnEscortCircleClosesRoundItsGroup)
{
  Vec2 const centre{ 0.2f, -0.1f };
  auto const points = escortPathPoints (centre, 0.3f, 48);
  ASSERT_EQ (points.size (), 49u);
  EXPECT_EQ (points.front (), points.back ()) << "closed";
  for (auto const &p : points)
    EXPECT_NEAR ((p - centre).getDistanceFromOrigin (), 0.3f, 1e-4f);
}

TEST (ShipPath, AnEscortCircleStaysOnTheShipsDisc)
{
  // Round a group past the rim the ship slides along the rim: so does its
  // circle.
  auto const points = escortPathPoints ({ 1.1f, 0.f }, 0.3f, 48);
  auto onTheRim = 0;
  for (auto const &p : points)
    {
      EXPECT_LE (p.getDistanceFromOrigin (), 1.f + 1e-5f);
      onTheRim += std::abs (p.getDistanceFromOrigin () - 1.f) < 1e-4f ? 1 : 0;
    }
  EXPECT_GT (onTheRim, 0);
}

TEST (ShipPath, NoPointsForNoCircle)
{
  EXPECT_TRUE (escortPathPoints ({ 0.f, 0.f }, 0.f, 48).empty ());
  EXPECT_TRUE (escortPathPoints ({ 0.f, 0.f }, 0.3f, 2).empty ());
}

namespace
{
FlightBodies
oneBody (int id, Vec2 at, float mass)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0].id = id;
  bodies.body[0].at = at;
  bodies.body[0].mass = mass;
  return bodies;
}
}

TEST (ShipPath, AnOrbitShipFliesTheGuide)
{
  std::vector<Vec2> const guide{ { 0.5f, 0.f }, { 0.f, 0.5f }, { -0.5f, 0.f },
                                 { 0.5f, 0.f } };
  auto const points = shipPathPoints (ShipPathShown::Orbit, guide, {},
                                      noBodyId, FlightTuning{});
  EXPECT_EQ (points, guide);
}

TEST (ShipPath, AnEscortFliesItsCircleRoundItsGroup)
{
  FlightTuning const t;
  auto const bodies = oneBody (3, { 0.2f, 0.1f }, t.crowdMass);
  auto const points
      = shipPathPoints (ShipPathShown::Escort, {}, bodies, 3, t);
  EXPECT_EQ (points, escortPathPoints ({ 0.2f, 0.1f },
                                      escortRadius (t.crowdMass, t),
                                      escortPathPointCount));
  EXPECT_TRUE (shipPathPoints (ShipPathShown::Escort, {}, bodies, 5, t).empty ())
      << "a group that is gone has no circle";
}

TEST (ShipPath, NeitherATrajectoryNorAGameHasFloorPoints)
{
  std::vector<Vec2> const guide{ { 0.5f, 0.f }, { 0.f, 0.5f } };
  EXPECT_TRUE (shipPathPoints (ShipPathShown::Trajectory, guide, {}, noBodyId,
                               FlightTuning{})
                   .empty ());
  EXPECT_TRUE (shipPathPoints (ShipPathShown::None, guide, {}, noBodyId,
                               FlightTuning{})
                   .empty ());
}
