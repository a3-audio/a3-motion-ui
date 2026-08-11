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

#include "EnergyMap.hh"

#include "SpeakerLightScaling.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{

constexpr float pi = 3.14159265358979323846f;
constexpr float degToRad = pi / 180.f;

// Below this share of a texel's strongest contribution a grid point changes
// nothing visible, and dropping it keeps the weight table small.
constexpr float contributionThreshold = 0.01f;

struct Cartesian
{
  float x, y, z;
};

Cartesian
toCartesian (float azimuthDegrees, float elevationDegrees)
{
  auto const az = azimuthDegrees * degToRad;
  auto const el = elevationDegrees * degToRad;
  auto const cosEl = std::cos (el);

  return { cosEl * std::cos (az), cosEl * std::sin (az), std::sin (el) };
}

/** Direction at the centre of a texel. */
Cartesian
texelDirection (int texel)
{
  auto const column = texel % energyMapWidth;
  auto const row = texel / energyMapWidth;

  auto const azimuth = (column + 0.5f) / energyMapWidth * 360.f - 180.f;
  auto const elevation = (row + 0.5f) / energyMapHeight * 180.f - 90.f;

  return toCartesian (azimuth, elevation);
}

float
dot (Cartesian const &a, Cartesian const &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

}

std::vector<EnergyDirection>
loadEnergyGrid (juce::File const &jsonFile)
{
  auto const parsed = juce::JSON::parse (jsonFile.loadFileAsString ());
  auto const *azimuth = parsed["azimuthInDegrees"].getArray ();
  auto const *elevation = parsed["elevationInDegrees"].getArray ();

  if (azimuth == nullptr || elevation == nullptr
      || azimuth->size () != elevation->size ())
    return {};

  std::vector<EnergyDirection> grid;
  grid.reserve (static_cast<size_t> (azimuth->size ()));

  for (int i = 0; i < azimuth->size (); ++i)
    grid.push_back ({ static_cast<float> ((*azimuth)[i]),
                      static_cast<float> ((*elevation)[i]) });

  return grid;
}

EnergyDirection
energyDirectionForScreen (float x, float y)
{
  auto const radius = std::min (std::hypot (x, y), 1.f);

  // Orthographic: the height above the horizontal plane is what is left of the
  // unit sphere, so elevation is asin of it rather than linear in radius.
  auto const elevation = std::asin (std::sqrt (std::max (0.f, 1.f - radius * radius)));

  // Follows the blob pipeline: cartesian2DHOA2JUCE puts a position at
  // { -y, -x } with JUCE's y pointing down, which lands azimuth 0 at the top
  // of the disc and runs it anticlockwise.
  auto const azimuth = std::atan2 (-x, y) / degToRad;

  return { azimuth, elevation / degToRad };
}

NetDomainPoint
netDomainPoint (float x, float y, float radial, float twist, float scale)
{
  auto const length = std::hypot (x, y);
  auto const nx = (length > 1e-6f) ? x / length : 0.f;
  auto const ny = (length > 1e-6f) ? y / length : 0.f;

  return { nx * twist, ny * twist, radial * scale };
}

float
beamRimCoverageDegrees (float angleDegrees, float speakerRadius)
{
  auto const mouthRadius = speakerMouthRadius (speakerRadius);
  auto const spread = std::tan (std::min (angleDegrees, 85.f) * degToRad);

  // Walk out from the axis until the rim leaves the beam. The rim point at
  // offset phi sits sin(phi) off the axis and mouthRadius - cos(phi) along it.
  auto covered = 0.f;
  for (auto phi = 0.f; phi <= 90.f; phi += 0.5f)
    {
      auto const rad = phi * degToRad;
      auto const across = std::sin (rad);
      auto const along = mouthRadius - std::cos (rad);
      auto const halfWidth
          = beamHalfWidthAt (along, speakerApertureHalfWidth, spread);

      if (across > halfWidth)
        break;

      covered = phi;
    }

  return covered * 2.f; // both sides of the axis
}

float
netFilamentRadius (float filamentCoordinate, float time, float flow)
{
  return filamentCoordinate - time * flow;
}

float
glowEmergence (float distanceFromCentre, float rise)
{
  auto const t = std::clamp ((distanceFromCentre - 1.f)
                                 / std::max (rise, 1e-4f),
                             0.f, 1.f);

  return t * t * (3.f - 2.f * t); // smoothstep, matching GLSL
}

float
beamWrapHalfAngle (float distanceFromCentre, float mouthRadius,
                   float apertureAngleDegrees, float wrapAngleDegrees)
{
  auto const span = std::max (mouthRadius - 1.f, 1e-4f);
  auto const t
      = std::clamp ((mouthRadius - distanceFromCentre) / span, 0.f, 1.f);
  auto const eased = t * t * (3.f - 2.f * t); // smoothstep, matching GLSL

  return apertureAngleDegrees
         + eased * (wrapAngleDegrees - apertureAngleDegrees);
}

float
beamBandLevel (float level, float floor)
{
  return floor + std::clamp (level, 0.f, 1.f) * (1.f - floor);
}

float
beamRadialWindow (float distanceFromCentre, float mouthRadius, float bleed)
{
  auto const span = std::max (bleed, 1e-4f);

  auto const outside
      = std::clamp ((mouthRadius + span - distanceFromCentre) / span, 0.f, 1.f);
  auto const inside
      = std::clamp ((distanceFromCentre - (1.f - span)) / span, 0.f, 1.f);

  auto const smooth = [] (float t) { return t * t * (3.f - 2.f * t); };

  return smooth (outside) * smooth (inside);
}

int
energyMapTexel (float azimuthDegrees, float elevationDegrees)
{
  auto const u = (azimuthDegrees + 180.f) / 360.f;
  auto const v = (elevationDegrees + 90.f) / 180.f;

  auto const column = std::clamp (static_cast<int> (u * energyMapWidth), 0,
                                  energyMapWidth - 1);
  auto const row = std::clamp (static_cast<int> (v * energyMapHeight), 0,
                               energyMapHeight - 1);

  return row * energyMapWidth + column;
}

EnergyMapProjection::EnergyMapProjection (
    std::vector<EnergyDirection> const &grid, float spreadDegrees)
{
  std::vector<Cartesian> directions;
  directions.reserve (grid.size ());
  for (auto const &point : grid)
    directions.push_back (
        toCartesian (point.azimuthDegrees, point.elevationDegrees));

  auto const spread = std::max (spreadDegrees, 1.f) * degToRad;

  _offsets.reserve (energyMapTexelCount + 1);
  _offsets.push_back (0);

  std::vector<float> weights (grid.size ());

  for (int texel = 0; texel < energyMapTexelCount; ++texel)
    {
      auto const direction = texelDirection (texel);

      auto strongest = 0.f;
      for (size_t i = 0; i < directions.size (); ++i)
        {
          auto const angle
              = std::acos (std::clamp (dot (direction, directions[i]), -1.f, 1.f));
          auto const t = angle / spread;
          weights[i] = std::exp (-t * t);
          strongest = std::max (strongest, weights[i]);
        }

      auto const cutoff = strongest * contributionThreshold;
      auto total = 0.f;
      for (auto const weight : weights)
        if (weight >= cutoff)
          total += weight;

      for (size_t i = 0; i < weights.size (); ++i)
        if (weights[i] >= cutoff)
          _contributions.push_back (
              { static_cast<int> (i), weights[i] / total });

      _offsets.push_back (static_cast<int> (_contributions.size ()));
    }
}

void
EnergyMapProjection::project (float const *gridValues, float *map) const
{
  for (int texel = 0; texel < energyMapTexelCount; ++texel)
    {
      auto sum = 0.f;
      for (auto i = _offsets[static_cast<size_t> (texel)];
           i < _offsets[static_cast<size_t> (texel) + 1]; ++i)
        {
          auto const &contribution = _contributions[static_cast<size_t> (i)];
          sum += gridValues[contribution.gridIndex] * contribution.weight;
        }

      map[texel] = sum;
    }
}

}
