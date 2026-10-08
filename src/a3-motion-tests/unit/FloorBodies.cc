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

#include <a3-motion-ui/components/fpv/FloorBodies.hh>

using namespace a3;

TEST (FloorBodies, ANinthGroupIsRefused)
{
  FloorBodies bodies;
  for (int i = 0; i < maxFlightBodies; ++i)
    EXPECT_TRUE (bodies.add ({ 0.1f * static_cast<float> (i), 0.f }).has_value ());
  EXPECT_FALSE (bodies.add ({ 0.5f, 0.5f }).has_value ());
  EXPECT_EQ (bodies.count (), maxFlightBodies);
}

TEST (FloorBodies, ANewGroupIsAGroup)
{
  FloorBodies bodies;
  auto const id = *bodies.add ({ 0.2f, 0.3f });
  EXPECT_EQ (bodies.weight (id), BodyWeight::Group);
  EXPECT_EQ (bodies.at (id), (Vec2{ 0.2f, 0.3f }));
}

TEST (FloorBodies, TheWeightCyclesThroughTheDeadZoneAndBack)
{
  EXPECT_EQ (nextWeight (BodyWeight::Group), BodyWeight::Crowd);
  EXPECT_EQ (nextWeight (BodyWeight::Crowd), BodyWeight::Hotspot);
  EXPECT_EQ (nextWeight (BodyWeight::Hotspot), BodyWeight::DeadZone);
  EXPECT_EQ (nextWeight (BodyWeight::DeadZone), BodyWeight::Group);

  FloorBodies bodies;
  auto const id = *bodies.add ({});
  for (auto want : { BodyWeight::Crowd, BodyWeight::Hotspot,
                     BodyWeight::DeadZone, BodyWeight::Group })
    {
      bodies.cycleWeight (id);
      EXPECT_EQ (bodies.weight (id), want);
    }
}

TEST (FloorBodies, AMoveStaysInTheRoom)
{
  FloorBodies bodies;
  auto const id = *bodies.add ({});
  bodies.move (id, { 3.f, 4.f });
  EXPECT_LE (bodies.at (id).getDistanceFromOrigin (), 1.f + 1e-6f);
  EXPECT_NEAR (bodies.at (id).x / bodies.at (id).y, 0.75f, 1e-5f)
      << "pulled straight in, same direction";

  bodies.move (id, { 0.3f, -0.4f });
  EXPECT_EQ (bodies.at (id), (Vec2{ 0.3f, -0.4f }));

  auto const outside = *bodies.add ({ -2.f, 0.f });
  EXPECT_LE (bodies.at (outside).getDistanceFromOrigin (), 1.f + 1e-6f);
}

TEST (FloorBodies, RemovingShiftsTheRestDownAndKeepsTheirIds)
{
  FloorBodies bodies;
  auto const a = *bodies.add ({ 0.1f, 0.f });
  auto const b = *bodies.add ({ 0.2f, 0.f });
  auto const c = *bodies.add ({ 0.3f, 0.f });
  bodies.cycleWeight (c);

  bodies.remove (b);

  auto const s = bodies.snapshot ();
  ASSERT_EQ (s.count, 2);
  EXPECT_EQ (s.body[0].id, a);
  EXPECT_EQ (s.body[1].id, c);
  EXPECT_EQ (s.body[1].at, (Vec2{ 0.3f, 0.f }));
  EXPECT_FLOAT_EQ (s.body[1].mass, massOf (BodyWeight::Crowd));
  EXPECT_FALSE (bodies.contains (b));
  EXPECT_TRUE (bodies.contains (c));
}

TEST (FloorBodies, AnIdStaysWithItsBodyWhileItLives)
{
  FloorBodies bodies;
  auto const a = *bodies.add ({ 0.1f, 0.f });
  auto const b = *bodies.add ({ 0.2f, 0.f });
  bodies.remove (a);
  EXPECT_EQ (bodies.at (b), (Vec2{ 0.2f, 0.f }))
      << "an escort that names b still finds b";
  bodies.cycleWeight (b);
  EXPECT_EQ (bodies.weight (b), BodyWeight::Crowd);
}

// The label is the id counted from one, so it must stay within G1..G8: the
// lowest free id is handed out again.
TEST (FloorBodies, AFreedNumberIsTakenByTheNextGroup)
{
  FloorBodies bodies;
  for (int i = 0; i < maxFlightBodies; ++i)
    bodies.add ({});
  bodies.remove (2);
  bodies.remove (5);
  EXPECT_EQ (bodies.add ({}), 2);
  EXPECT_EQ (bodies.add ({}), 5);
  EXPECT_FALSE (bodies.add ({}).has_value ());
}

TEST (FloorBodies, AnUnknownIdChangesNothing)
{
  FloorBodies bodies;
  auto const id = *bodies.add ({ 0.5f, 0.f });
  bodies.move (id + 1, { 0.f, 0.f });
  bodies.cycleWeight (noBodyId);
  bodies.remove (7);
  auto const s = bodies.snapshot ();
  ASSERT_EQ (s.count, 1);
  EXPECT_EQ (s.body[0].at, (Vec2{ 0.5f, 0.f }));
  EXPECT_FLOAT_EQ (s.body[0].mass, massOf (BodyWeight::Group));
}

TEST (FloorBodies, TheSnapshotCarriesMassesInOrder)
{
  FloorBodies bodies;
  auto const a = *bodies.add ({ 0.1f, 0.f });
  auto const b = *bodies.add ({ 0.2f, 0.f });
  auto const c = *bodies.add ({ 0.3f, 0.f });
  bodies.cycleWeight (b);
  bodies.cycleWeight (c);
  bodies.cycleWeight (c);
  bodies.cycleWeight (c);

  auto const s = bodies.snapshot ();
  ASSERT_EQ (s.count, 3);
  EXPECT_FLOAT_EQ (s.body[0].mass, 0.5f);
  EXPECT_FLOAT_EQ (s.body[1].mass, 1.f);
  EXPECT_FLOAT_EQ (s.body[2].mass, -2.f);
  EXPECT_EQ (s.body[0].id, a);
  EXPECT_EQ (s.body[1].id, b);
  EXPECT_EQ (s.body[2].id, c);
  EXPECT_EQ (s.body[2].at, (Vec2{ 0.3f, 0.f }));
}

// The MJ lab (2026-10-08): light planets, half the plan's masses. With the
// breath on, every body cost attention in the model; light ones cost least.
TEST (FloorBodies, PlanetsAreLight)
{
  EXPECT_FLOAT_EQ (massOf (BodyWeight::Group), 0.5f);
  EXPECT_FLOAT_EQ (massOf (BodyWeight::Crowd), 1.f);
  EXPECT_FLOAT_EQ (massOf (BodyWeight::Hotspot), 1.5f);
  EXPECT_FLOAT_EQ (massOf (BodyWeight::DeadZone), -2.f);
}
