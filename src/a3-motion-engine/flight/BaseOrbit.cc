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

#include "BaseOrbit.hh"

#include <a3-motion-engine/util/Geometry.hh>

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{

constexpr double twoPi = 2. * pi<double> ();
constexpr int coarseSamples = 64;
constexpr int refinementSteps = 8;

/** The ellipse as it stands at one moment: its semi-axes and how far it has
 *  turned. */
struct Ellipse
{
  float longAxis;
  float shortAxis;
  float turn; // radians
};

double
lapBeats (int beatsPerBar, FlightTuning const &tuning)
{
  return static_cast<double> (tuning.orbitLapBars) * beatsPerBar;
}

double
precessionBeats (int beatsPerBar, FlightTuning const &tuning)
{
  return static_cast<double> (tuning.orbitPrecessionBars) * beatsPerBar;
}

/** How many periods fit into `beats`; none when the period is not positive
 *  (a lap or precession of 0 bars means: that motion stands still). */
double
periodsIn (double beats, double periodBeats)
{
  return periodBeats > 0. ? beats / periodBeats : 0.;
}

/** Radians per beat of one turn per period; 0 for a period that is not
 *  positive, as in periodsIn. */
float
turnRate (double periodBeats)
{
  return periodBeats > 0. ? static_cast<float> (twoPi / periodBeats) : 0.f;
}

/** Only the fraction: an hour into a set the turn is still exact in float. */
float
turnAngle (double beats, double periodBeats)
{
  auto const periods = periodsIn (beats, periodBeats);
  return static_cast<float> (twoPi * (periods - std::floor (periods)));
}

Ellipse
ellipseAt (double beats, int beatsPerBar, FlightTuning const &tuning)
{
  return { tuning.orbitRadius * (1.f + tuning.orbitEccentricity),
           tuning.orbitRadius * (1.f - tuning.orbitEccentricity),
           turnAngle (beats, precessionBeats (beatsPerBar, tuning)) };
}

Vec2
rotate (Vec2 v, float angle)
{
  auto const c = std::cos (angle);
  auto const s = std::sin (angle);
  return { c * v.x - s * v.y, s * v.x + c * v.y };
}

/** The point at parameter `theta` in the ellipse's own (unturned) frame. */
Vec2
onEllipse (Ellipse const &e, float theta)
{
  return { e.longAxis * std::cos (theta), e.shortAxis * std::sin (theta) };
}

/** d/dtheta of |onEllipse - q|^2, halved: its zero is the nearest point. */
float
distanceSlope (Ellipse const &e, float theta, Vec2 q)
{
  auto const towards = onEllipse (e, theta) - q;
  Vec2 const tangent{ -e.longAxis * std::sin (theta),
                      e.shortAxis * std::cos (theta) };
  return towards.getDotProduct (tangent);
}

float
wrapPhase (double phase)
{
  auto const fraction = static_cast<float> (phase - std::floor (phase));
  return fraction < 1.f ? fraction : 0.f;
}

}

float
rabbitSlotPhase (double beats, int channel, int beatsPerBar,
                 FlightTuning const &tuning)
{
  return wrapPhase (periodsIn (beats, lapBeats (beatsPerBar, tuning))
                    - 0.25 * channel);
}

OrbitPoint
rabbitAt (double beats, int channel, int beatsPerBar,
          FlightTuning const &tuning, float phaseOffset)
{
  auto const lap = lapBeats (beatsPerBar, tuning);
  auto const ellipse = ellipseAt (beats, beatsPerBar, tuning);
  auto const phase = wrapPhase (static_cast<double> (
      rabbitSlotPhase (beats, channel, beatsPerBar, tuning) + phaseOffset));
  auto const theta = static_cast<float> (twoPi) * phase;

  auto const local = onEllipse (ellipse, theta);
  auto const thetaRate = turnRate (lap);
  auto const pathTurnRate = turnRate (precessionBeats (beatsPerBar, tuning));
  // The point's own motion along the ellipse, plus the ellipse turning
  // under it: d/dt R(turn) q(theta) = R(turn) (turn' J q + theta' q').
  Vec2 const alongPath{ -ellipse.longAxis * std::sin (theta) * thetaRate,
                        ellipse.shortAxis * std::cos (theta) * thetaRate };
  Vec2 const withTurn{ -local.y * pathTurnRate, local.x * pathTurnRate };

  return { rotate (local, ellipse.turn),
           rotate (alongPath + withTurn, ellipse.turn) };
}

float
nearestOrbitPhase (Vec2 p, double beats, int beatsPerBar,
                   FlightTuning const &tuning)
{
  auto const ellipse = ellipseAt (beats, beatsPerBar, tuning);
  auto const q = rotate (p, -ellipse.turn);
  auto const step = static_cast<float> (twoPi / coarseSamples);

  auto bestTheta = 0.f;
  auto bestDistance = onEllipse (ellipse, 0.f).getDistanceFrom (q);
  for (auto i = 1; i < coarseSamples; ++i)
    {
      auto const theta = step * static_cast<float> (i);
      auto const distance = onEllipse (ellipse, theta).getDistanceFrom (q);
      if (distance < bestDistance)
        {
          bestDistance = distance;
          bestTheta = theta;
        }
    }

  // The minimum lies within a sample either side; bisect on the slope.
  auto low = bestTheta - step;
  auto high = bestTheta + step;
  for (auto i = 0; i < refinementSteps; ++i)
    {
      auto const middle = 0.5f * (low + high);
      if (distanceSlope (ellipse, middle, q) < 0.f)
        low = middle;
      else
        high = middle;
    }

  return wrapPhase (0.5 * (low + high) / twoPi);
}

std::vector<Vec2>
orbitGuidePoints (double beats, int beatsPerBar, int count,
                  FlightTuning const &tuning)
{
  std::vector<Vec2> points;
  if (count <= 0)
    return points;

  auto const ellipse = ellipseAt (beats, beatsPerBar, tuning);
  auto const segments = std::max (count - 1, 1);
  points.reserve (static_cast<size_t> (count));
  for (auto i = 0; i < count; ++i)
    {
      auto const theta = static_cast<float> (twoPi * i / segments);
      points.push_back (rotate (onEllipse (ellipse, theta), ellipse.turn));
    }
  return points;
}

}
