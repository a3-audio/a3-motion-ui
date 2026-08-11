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

  // The screen angle runs anticlockwise from the right, the IEM azimuth
  // clockwise from the top — hence 90 minus, not plus.
  auto const screenAngle = std::atan2 (y, x);
  auto azimuth = 90.f - screenAngle / degToRad;
  while (azimuth > 180.f)
    azimuth -= 360.f;
  while (azimuth <= -180.f)
    azimuth += 360.f;

  return { azimuth, elevation / degToRad };
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
