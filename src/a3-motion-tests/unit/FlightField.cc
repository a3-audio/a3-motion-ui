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

#include <a3-motion-engine/flight/FlightField.hh>

#include <cmath>

using namespace a3;

TEST (FlightField, AGroupPullsTowardsItself)
{
  FlightBodies b;
  b.body[0] = { { 0.5f, 0.f }, 1.f };
  b.count = 1;
  auto const a = gravityAt ({ 0.f, 0.f }, b, 1.f, FlightTuning{});
  EXPECT_GT (a.x, 0.f);
  EXPECT_NEAR (a.y, 0.f, 1e-6f);
}

TEST (FlightField, AHeavierGroupPullsHarder)
{
  FlightBodies light, heavy;
  light.body[0] = { { 0.5f, 0.f }, 1.f };
  light.count = 1;
  heavy.body[0] = { { 0.5f, 0.f }, 3.f };
  heavy.count = 1;
  FlightTuning const t;
  EXPECT_GT (gravityAt ({}, heavy, 1.f, t).x, gravityAt ({}, light, 1.f, t).x);
}

TEST (FlightField, ADeadZonePushesAway)
{
  FlightBodies b;
  b.body[0] = { { 0.5f, 0.f }, -2.f };
  b.count = 1;
  EXPECT_LT (gravityAt ({}, b, 1.f, FlightTuning{}).x, 0.f);
}

TEST (FlightField, OnTopOfABodyThePullIsFiniteAndCapped)
{
  FlightBodies b;
  b.body[0] = { { 0.3f, 0.3f }, 3.f };
  b.count = 1;
  FlightTuning const t;
  auto const a = gravityAt ({ 0.3f, 0.3f }, b, 1.6f, t);
  EXPECT_TRUE (std::isfinite (a.x) && std::isfinite (a.y));
  EXPECT_LE (a.getDistanceFromOrigin (), t.gravityMax + 1e-5f);
}

TEST (FlightField, BodiesBeyondTheCountAreIgnored)
{
  FlightBodies b;
  b.body[3] = { { 0.5f, 0.f }, 3.f };
  b.count = 0;
  EXPECT_EQ (gravityAt ({}, b, 1.f, FlightTuning{}), Vec2{});
}

TEST (FlightField, ThePulseScalesThePull)
{
  FlightBodies b;
  b.body[0] = { { 0.6f, 0.f }, 1.f };
  b.count = 1;
  FlightTuning const t;
  EXPECT_NEAR (gravityAt ({}, b, 1.3f, t).x,
               1.3f * gravityAt ({}, b, 1.f, t).x, 1e-6f);
}

TEST (FlightField, ZeroSofteningOnTopOfABodyIsFinite)
{
  FlightBodies b;
  b.body[0] = { { 0.3f, 0.3f }, 3.f };
  b.count = 1;
  FlightTuning t;
  t.softening = 0.f;
  for (auto const p : { Vec2{ 0.3f, 0.3f }, Vec2{ 0.3f + 1e-6f, 0.3f } })
    {
      auto const a = gravityAt (p, b, 1.6f, t);
      EXPECT_TRUE (std::isfinite (a.x) && std::isfinite (a.y));
      EXPECT_LE (a.getDistanceFromOrigin (), t.gravityMax + 1e-5f);
    }
}
