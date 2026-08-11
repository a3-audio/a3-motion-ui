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

#pragma once

#include <JuceHeader.h>

#include <vector>

namespace a3
{

/** One direction of the IEM EnergyVisualizer's grid.
 *
 *  The plugin sends `/EnergyVisualizer/RMS` with one float per grid point, in
 *  the order of its own coordinate file — so the order here is the plugin's,
 *  not ours. */
struct EnergyDirection
{
  float azimuthDegrees = 0.f;
  float elevationDegrees = 0.f;
};

std::vector<EnergyDirection> loadEnergyGrid (juce::File const &jsonFile);

// Equirectangular map the grid is folded into, small enough to rebuild every
// update and to upload as a texture without stalling the render thread.
constexpr int energyMapWidth = 64;
constexpr int energyMapHeight = 32;
constexpr int energyMapTexelCount = energyMapWidth * energyMapHeight;

// 426 points over a sphere sit roughly 10 degrees apart, so this is about one
// grid spacing: enough to close the gaps between points without washing
// neighbouring directions into each other.
constexpr float energyMapSpreadDegrees = 12.f;

/** Direction a point of the sphere display stands for.
 *
 *  The display is an orthographic view of the upper hemisphere from above:
 *  the centre of the disc is straight up, the rim is the horizon. That is what
 *  the shader's own sphere normal already describes, and what puts the blobs
 *  on the rim rather than half way out.
 *
 *  `x`/`y` are in sphere-normalised screen coordinates with y pointing up.
 *  Energy from below the horizon has nowhere to go in this projection and is
 *  not shown.
 *
 *  Azimuth is the one the blobs are sent out with — Position::azimuth() as it
 *  reaches /StereoEncoder/azimuth — so the energy lands where the blob for
 *  that direction sits: 0 at the top of the disc, running anticlockwise. The
 *  speaker icons are not a reference for this; drawSpeakers() places them by
 *  raw angle, without the HOA-to-JUCE conversion the blobs go through. */
EnergyDirection energyDirectionForScreen (float x, float y);

/** Index of the texel a direction falls in. */
int energyMapTexel (float azimuthDegrees, float elevationDegrees);

/** Folds the grid's 426 values into the equirectangular map.
 *
 *  The weights are a spherical Gaussian, resolved once at construction — a
 *  nearest-point lookup instead would show the grid as facets. `spreadDegrees`
 *  is how far a single point's energy reaches; too small leaves holes between
 *  grid points, too large smears the whole sphere into one colour. */
class EnergyMapProjection
{
public:
  EnergyMapProjection (std::vector<EnergyDirection> const &grid,
                       float spreadDegrees);

  /** `gridValues` holds one value per grid point, `map` receives
   *  energyMapTexelCount values. Both are caller-owned. */
  void project (float const *gridValues, float *map) const;

private:
  struct Contribution
  {
    int gridIndex;
    float weight;
  };

  // One run of contributions per texel, indexed by _offsets[texel] ..
  // _offsets[texel + 1].
  std::vector<Contribution> _contributions;
  std::vector<int> _offsets;
};

}
