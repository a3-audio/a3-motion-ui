/*

  A3 Motion UI
  Copyright (C) 2023 Patric Schmitz

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

#include "SpeakerLightScaling.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

float
speakerLightLevel (float vuRms, float vuMax, float curve)
{
  return std::pow (std::clamp (vuRms / vuMax, 0.f, 1.f), curve);
}

float
speakerLightEnvelope (float current, float target, float attackSeconds,
                      float decaySeconds, float dt)
{
  auto const tau = (target > current) ? attackSeconds : decaySeconds;
  auto const alpha = 1.f - std::exp (-dt / std::max (0.001f, tau));

  return current + alpha * (target - current);
}

float
beamHalfAngleDegrees (float width)
{
  auto constexpr radToDeg = 180.f / 3.14159265358979323846f;

  return std::acos (std::clamp (1.f - width, -1.f, 1.f)) * radToDeg;
}

namespace
{
// tan runs to infinity at 90 degrees. A beam that grazes its own axis plane is
// already wider than the picture, so cutting it short here costs nothing and
// keeps a config typo from producing a NaN.
constexpr float maxBeamAngleDegrees = 85.f;
constexpr float degToRad = 3.14159265358979323846f / 180.f;
}

float
coneWidthFromAngle (float angleDegrees)
{
  return 1.f - std::cos (std::clamp (angleDegrees, 0.f, 180.f) * degToRad);
}

float
beamAngleAtLevel (float level, float quietAngleDegrees,
                  float loudAngleDegrees)
{
  auto const t = std::clamp (level, 0.f, 1.f);

  return quietAngleDegrees + t * (loudAngleDegrees - quietAngleDegrees);
}

float
beamSpreadTangent (float width)
{
  auto const angle = std::min (beamHalfAngleDegrees (width),
                               maxBeamAngleDegrees);

  return std::tan (angle * degToRad);
}

float
beamProfile (float offset, float halfWidth, float edgeSoftness)
{
  auto const edge = std::max (halfWidth, 1e-6f);
  auto const flat = edge * std::clamp (edgeSoftness, 0.f, 0.999f);

  auto const t = std::clamp ((std::abs (offset) - flat) / (edge - flat), 0.f,
                             1.f);

  return 1.f - t * t * (3.f - 2.f * t); // smoothstep, matching GLSL
}

float
beamHalfWidthAt (float axialDistance, float apertureHalfWidth,
                 float spreadTangent)
{
  if (axialDistance < 0.f)
    return 0.f;

  return apertureHalfWidth + axialDistance * spreadTangent;
}

float
beamPathInsideSphere (float axialDistance, float perpendicularOffset,
                      float mouthRadius)
{
  auto const length = std::hypot (axialDistance, perpendicularOffset);
  if (length < 1e-6f)
    return 0.f;

  // Distance from the sphere centre to the ray, via the projection of the
  // centre onto it.
  auto const alongRay = mouthRadius * axialDistance / length;
  auto const missDistanceSquared = mouthRadius * mouthRadius - alongRay * alongRay;
  if (missDistanceSquared >= 1.f)
    return 0.f;

  auto const halfChord = std::sqrt (1.f - missDistanceSquared);
  auto const entry = alongRay - halfChord;
  auto const exit = alongRay + halfChord;

  return std::max (0.f, std::min (length, exit) - std::max (entry, 0.f));
}

float
beamAbsorption (float pathLength, float coefficient)
{
  return std::exp (-coefficient * std::max (pathLength, 0.f));
}

float
sphereHalfChord (float distanceFromCentre)
{
  auto const d = std::clamp (distanceFromCentre, 0.f, 1.f);

  return std::sqrt (1.f - d * d);
}

}
