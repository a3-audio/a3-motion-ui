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

#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/flight/ShipHearing.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

using namespace a3;

// Where a flying ship is heard while an action drives it (phase B): its floor
// point through its clip's band, swung in height and leant -- and never turning
// round the listener faster than a room can follow.

namespace
{
constexpr int fourFour = 4;

double
degreesBetween (Pos const &a, Pos const &b)
{
  auto const dot = a.x () * b.x () + a.y () * b.y () + a.z () * b.z ();
  auto const cosine = std::clamp (static_cast<double> (dot / (a.distance () * b.distance ())), -1., 1.);
  return std::acos (cosine) * 180. / juce::MathConstants<double>::pi;
}

Pos const aboveTheFront = Pos::fromSpherical (0.f, 20.f, 1.f);
}

TEST (ShipHearing, WithoutAnActionTheShipIsHeardWhereItFlies)
{
  FlightTuning const tuning;
  EXPECT_EQ (heardShip (aboveTheFront, FlightMotion{}, 1.f, 3., fourFour, tuning), aboveTheFront);
  FlightMotion leant;
  leant.tilt = 1.f;
  EXPECT_EQ (heardShip (aboveTheFront, leant, 0.f, 3., fourFour, tuning), aboveTheFront)
      << "an action with no hold yet changes nothing";
}

TEST (ShipHearing, ASwayLowersThenLifts)
{
  FlightTuning const tuning;
  FlightMotion sway;
  sway.sway = 5; // two bars
  auto const lowest = heardShip (aboveTheFront, sway, 1.f, 4., fourFour, tuning);
  EXPECT_NEAR (lowest.elevation (), 20.f - tuning.swayTravelDegrees, 0.05f);
  EXPECT_NEAR (lowest.azimuth (), 0.f, 0.05f);
}

TEST (ShipHearing, ATiltLeansTheHeardPlane)
{
  FlightMotion leant;
  leant.tilt = 1.f;
  auto const heard = heardShip (aboveTheFront, leant, 1.f, 3., fourFour, FlightTuning{});
  EXPECT_LT (degreesBetween (heard, turnedInSpace (aboveTheFront, { 1.f, 0.f })), 0.01);
}

TEST (ShipHearing, HalfHeldIsHalfwayThere)
{
  FlightMotion leant;
  leant.roll = 1.f;
  FlightTuning const tuning;
  auto const full = heardShip (aboveTheFront, leant, 1.f, 3., fourFour, tuning);
  auto const half = heardShip (aboveTheFront, leant, 0.5f, 3., fourFour, tuning);
  EXPECT_NEAR (degreesBetween (aboveTheFront, half), 0.5 * degreesBetween (aboveTheFront, full), 0.5);
}

TEST (ShipHearing, TheTurnIsLimited)
{
  auto const from = Pos::fromSpherical (0.f, 0.f, 1.f);
  auto const to = Pos::fromSpherical (90.f, 0.f, 1.f);
  auto const far = limitTurn (from, to, 10.f);
  EXPECT_NEAR (degreesBetween (from, far.heard), 10., 0.01);
  EXPECT_NEAR (degreesBetween (far.heard, to), 80., 0.01) << "along the great circle";
  EXPECT_FALSE (far.caughtUp);

  auto const near = limitTurn (from, Pos::fromSpherical (4.f, 0.f, 1.f), 10.f);
  EXPECT_TRUE (near.caughtUp);
  EXPECT_LT (degreesBetween (near.heard, Pos::fromSpherical (4.f, 0.f, 1.f)), 1e-3);
}

TEST (ShipHearing, NoStartIsNoLimit)
{
  auto const to = Pos::fromSpherical (90.f, 0.f, 1.f);
  auto const out = limitTurn (Pos::invalid, to, 1.f);
  EXPECT_TRUE (out.caughtUp);
  EXPECT_EQ (out.heard, to);
}

// Everything at once, eased in over a beat as the world eases: the limiter
// is what guarantees the cap, so a heard path built tick by tick never turns
// faster than it.
TEST (ShipHearing, EvenEverythingAtOnceStaysUnderTheCap)
{
  FlightTuning const tuning;
  FlightMotion wild;
  wild.tilt = 2.f;
  wild.tiltSweep = 8;
  wild.rollSweep = -8;
  wild.sway = 8;
  auto const ticksPerBeat = static_cast<double> (TempoClock::getTicksPerBeat ());
  auto const perTick = tuning.angularCapDegreesPerBeat / static_cast<float> (ticksPerBeat);

  auto heard = aboveTheFront;
  auto widest = 0.;
  for (auto tick = 0; tick < 4 * static_cast<int> (ticksPerBeat); ++tick)
    {
      auto const beats = tick / ticksPerBeat;
      auto const weight = static_cast<float> (std::min (1., beats));
      auto const next
          = limitTurn (heard, heardShip (aboveTheFront, wild, weight, beats, fourFour, tuning),
                       perTick).heard;
      widest = std::max (widest, degreesBetween (heard, next));
      heard = next;
    }
  EXPECT_LE (widest, static_cast<double> (perTick) * 1.001);
}
