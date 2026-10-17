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

#include <a3-motion-engine/flight/FigureMath.hh>
#include <a3-motion-engine/flight/SlapGlide.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

#include <algorithm>
#include <cmath>

using namespace a3;

// A slap's floor point jumps; the heard direction is led there in a quarter
// beat, along a way that keeps it on the stage: left-right through the
// front at one height, front-back round the near side, up-down in height
// only.

namespace
{
GameTuning const tuning;

Pos
heardAt (float azimuth, float elevation)
{
  return Pos::fromSpherical (azimuth, elevation, 1.f);
}

double
tick ()
{
  return 1. / TempoClock::getTicksPerBeat ();
}
}

TEST (SlapGlide, ItReachesTheSideWithinAQuarterBeat)
{
  auto const left = heardAt (90.f, 10.f);
  auto const right = heardAt (-90.f, 10.f);
  auto const length = slapGlideLength (GlidePath::ViaFront, tuning);
  EXPECT_DOUBLE_EQ (length, tuning.slapGlideBeats);
  EXPECT_LT (degreesBetween (slapGlideAt (left, right, GlidePath::ViaFront, length, tuning), right),
             1e-3);
  EXPECT_GT (degreesBetween (slapGlideAt (left, right, GlidePath::ViaFront, length / 2., tuning),
                             right),
             45.);
}

TEST (SlapGlide, LeftRightGoesThroughTheFrontAtOneHeight)
{
  for (auto const way : { 1.f, -1.f })
    {
      auto const from = heardAt (90.f * way, 10.f);
      auto const to = heardAt (-90.f * way, 10.f);
      for (auto u = 0.; u <= 1.; u += 0.05)
        {
          auto const p = slapGlideAt (from, to, GlidePath::ViaFront, u * tuning.slapGlideBeats,
                                      tuning);
          EXPECT_NEAR (p.elevation (), 10.f, 1e-3f) << u;
          EXPECT_LE (std::abs (p.azimuth ()), 90.f + 1e-3f) << "never round the back, " << u;
        }
      auto const half = slapGlideAt (from, to, GlidePath::ViaFront, tuning.slapGlideBeats / 2.,
                                     tuning);
      EXPECT_NEAR (half.azimuth (), 0.f, 1e-3f);
    }
}

TEST (SlapGlide, FrontBackGoesRoundTheSideTheShipIsHeardOn)
{
  auto const half = tuning.slapGlideBeats / 2.;
  auto const viaLeft = slapGlideAt (heardAt (5.f, 10.f), heardAt (180.f, 10.f), GlidePath::ViaSide,
                                    half, tuning);
  EXPECT_GT (viaLeft.azimuth (), 45.f);
  auto const viaRight = slapGlideAt (heardAt (-5.f, 10.f), heardAt (180.f, 10.f),
                                     GlidePath::ViaSide, half, tuning);
  EXPECT_LT (viaRight.azimuth (), -45.f);
}

TEST (SlapGlide, UpDownKeepsTheAzimuthAndTakesLonger)
{
  auto const from = heardAt (30.f, 80.f);
  auto const to = heardAt (30.f, 0.f);
  auto const length = slapGlideLength (GlidePath::Vertical, tuning);
  EXPECT_DOUBLE_EQ (length, tuning.slapGlideBeats / tuning.slapVerticalRateFactor);
  for (auto u = 0.; u < 1.; u += 0.1)
    EXPECT_NEAR (slapGlideAt (from, to, GlidePath::Vertical, u * length, tuning).azimuth (), 30.f,
                 1e-3f);
  EXPECT_GT (slapGlideAt (from, to, GlidePath::Vertical, tuning.slapGlideBeats, tuning).elevation (),
             1.f)
      << "not there at the horizontal glide's length";
}

TEST (SlapGlide, EachTickStepsOnlyItsShareOfTheWay)
{
  // Whatever the tempo, the glide is counted in ticks of the beat: at 174
  // BPM as at 100 it spreads over the same number of position updates.
  auto const from = heardAt (90.f, 10.f);
  auto const to = heardAt (-90.f, 10.f);
  auto const ticks = tuning.slapGlideBeats * TempoClock::getTicksPerBeat ();
  auto widest = 0.;
  auto last = from;
  for (auto beats = tick (); beats <= tuning.slapGlideBeats + 1e-9; beats += tick ())
    {
      auto const now = slapGlideAt (from, to, GlidePath::ViaFront, beats, tuning);
      widest = std::max (widest, degreesBetween (last, now));
      last = now;
    }
  EXPECT_LE (widest, 180. / ticks + 0.1);
}

TEST (SlapGlider, ANewHoldPointStartsAGlideAndItsEndHandsBackToTheTurnLimit)
{
  SlapGlider glider;
  FigureStance const held{ true, false, GlidePath::ViaFront };
  auto const front = heardAt (0.f, 10.f);
  auto const left = heardAt (90.f, 10.f);
  auto const first = glider.step (held, { 0.f, 0.85f }, front, left, 8., tuning);
  ASSERT_TRUE (first.has_value ()) << "a new hold point starts a glide";
  EXPECT_LT (degreesBetween (*first, front), 1e-3) << "from where it was heard";
  auto const landed = glider.step (held, { 0.f, 0.85f }, *first, left,
                                   8. + tuning.slapGlideBeats, tuning);
  ASSERT_TRUE (landed.has_value ());
  EXPECT_LT (degreesBetween (*landed, left), 1e-3) << "the last glide tick is the side itself";
  EXPECT_FALSE (glider.step (held, { 0.f, 0.85f }, left, left, 8.5, tuning).has_value ())
      << "standing on: the turn limit takes over again";
}

TEST (SlapGlider, AGlideFromNowhereIsNoGlide)
{
  SlapGlider glider;
  FigureStance const held{ true, false, GlidePath::Shortest };
  EXPECT_FALSE (glider.step (held, { 0.85f, 0.f }, Pos::invalid, heardAt (0.f, 10.f), 8., tuning)
                    .has_value ());
}

TEST (SlapGlider, WithoutAHoldItDoesNothing)
{
  SlapGlider glider;
  EXPECT_FALSE (glider.step ({}, { 0.f, 0.7f }, heardAt (0.f, 0.f), heardAt (90.f, 0.f), 8., tuning)
                    .has_value ());
}
