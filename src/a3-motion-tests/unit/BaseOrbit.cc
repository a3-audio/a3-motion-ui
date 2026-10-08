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

#include <a3-motion-engine/flight/BaseOrbit.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/util/Geometry.hh>

#include <algorithm>
#include <cmath>

using namespace a3;

namespace
{

constexpr int fourFour = 4;
constexpr float twoPi = 2.f * pi<float> ();

float
angleOf (Vec2 v)
{
  return std::atan2 (v.y, v.x);
}

/** `a - b` wrapped into (-pi, pi]. */
float
angleBetween (float a, float b)
{
  return std::remainder (a - b, twoPi);
}

/** The circular distance between two lap phases in [0, 1). */
float
phaseDistance (float a, float b)
{
  auto const d = std::fabs (a - b);
  return std::min (d, 1.f - d);
}

double
lapBeats (FlightTuning const &t)
{
  return static_cast<double> (t.orbitLapBars) * fourFour;
}

/** The angle of the ellipse's long axis, folded into [0, pi). */
float
longAxisAngle (double beats, FlightTuning const &t)
{
  auto const points = orbitGuidePoints (beats, fourFour, 720, t);
  auto const farthest = std::max_element (
      points.begin (), points.end (), [] (Vec2 a, Vec2 b) {
        return a.getDistanceFromOrigin () < b.getDistanceFromOrigin ();
      });
  auto const angle = angleOf (*farthest);
  return angle < 0.f ? angle + pi<float> () : angle;
}

}

TEST (BaseOrbit, OneLapPerFourBars)
{
  FlightTuning const t;
  auto const lap = lapBeats (t);
  auto const start = rabbitAt (0., 0, fourFour, t).at;
  auto const afterALap = rabbitAt (lap, 0, fourFour, t).at;
  auto const precessionInALap = twoPi * t.orbitLapBars / t.orbitPrecessionBars;

  EXPECT_NEAR (afterALap.getDistanceFromOrigin (),
               start.getDistanceFromOrigin (), 1e-4f);
  EXPECT_NEAR (angleBetween (angleOf (afterALap), angleOf (start)),
               precessionInALap, 1e-3f);

  auto const halfway = rabbitAt (lap / 2., 0, fourFour, t).at;
  EXPECT_LT (halfway.getDotProduct (start), 0.f) << "not on the far side";
}

TEST (BaseOrbit, TheFourRabbitsAreAQuarterLapApart)
{
  FlightTuning const t;
  for (auto channel = 1; channel < 4; ++channel)
    {
      auto const ahead = angleOf (rabbitAt (0., channel - 1, fourFour, t).at);
      auto const behind = angleOf (rabbitAt (0., channel, fourFour, t).at);
      EXPECT_NEAR (angleBetween (ahead, behind), pi<float> () / 2.f, 0.15f)
          << "channel " << channel;
    }
}

TEST (BaseOrbit, TheRabbitStaysOnTheEllipse)
{
  FlightTuning const t;
  auto const inner = t.orbitRadius * (1.f - t.orbitEccentricity);
  auto const outer = t.orbitRadius * (1.f + t.orbitEccentricity);
  for (auto i = 0; i < 200; ++i)
    {
      auto const beats = 0.37 * i;
      auto const radius
          = rabbitAt (beats, i % 4, fourFour, t).at.getDistanceFromOrigin ();
      EXPECT_GE (radius, inner - 1e-5f) << "at " << beats;
      EXPECT_LE (radius, outer + 1e-5f) << "at " << beats;
    }
}

TEST (BaseOrbit, ItsVelocityIsTheDerivative)
{
  FlightTuning const t;
  auto const h = 1. / TempoClock::getTicksPerBeat ();
  for (auto i = 0; i < 40; ++i)
    {
      auto const beats = 0.83 * i;
      auto const channel = i % 4;
      auto const before = rabbitAt (beats - h, channel, fourFour, t).at;
      auto const after = rabbitAt (beats + h, channel, fourFour, t).at;
      auto const difference = (after - before) / static_cast<float> (2. * h);
      auto const velocity = rabbitAt (beats, channel, fourFour, t).velocity;
      EXPECT_LE (difference.getDistanceFrom (velocity),
                 0.01f * velocity.getDistanceFromOrigin ())
          << "at " << beats;
    }
}

TEST (BaseOrbit, ThePathTurnsSlowly)
{
  FlightTuning const t;
  auto const quarterTurnBeats
      = static_cast<double> (t.orbitPrecessionBars) / 4. * fourFour;
  auto const turned = longAxisAngle (quarterTurnBeats, t) - longAxisAngle (0., t);
  EXPECT_NEAR (std::fabs (std::remainder (turned, pi<float> ())),
               pi<float> () / 2.f, 0.02f);
}

TEST (BaseOrbit, NearestPhaseFindsTheRabbit)
{
  FlightTuning const t;
  for (auto const beats : { 0., 3.3, 10.7, 27.1, 61.9 })
    {
      auto const rabbit = rabbitAt (beats, 0, fourFour, t).at;
      auto const expected = static_cast<float> (
          std::fmod (beats / lapBeats (t), 1.));
      auto const found = nearestOrbitPhase (rabbit, beats, fourFour, t);
      EXPECT_GE (found, 0.f);
      EXPECT_LT (found, 1.f);
      EXPECT_LE (phaseDistance (found, expected), 1e-2f) << "at " << beats;
    }
}

TEST (BaseOrbit, GuidePointsCloseTheLoop)
{
  FlightTuning const t;
  auto const count = 64;
  auto const points = orbitGuidePoints (5.5, fourFour, count, t);
  ASSERT_EQ (points.size (), static_cast<size_t> (count));

  auto const longestStep
      = twoPi * t.orbitRadius * (1.f + t.orbitEccentricity) / (count - 1);
  EXPECT_LE (points.front ().getDistanceFrom (points.back ()), longestStep);
}
