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

/** Radius at which a given filament of a net stands at a given time.
 *
 *  `flow` is signed. Positive runs the filaments inwards — the direction sound
 *  arrives from — which is what the net inside the sphere does. Negative runs
 *  them outwards, which is what the glow outside it does. The shader picks the
 *  filament sitting at a pixel by inverting this. */
float netFilamentRadius (float filamentCoordinate, float time, float flow);

// How far out the screen corner sits, in sphere radii. The component is
// 768 x ~734 px with the sphere at reduceFactorCircle 0.64, so a radius of
// 734 * 0.64 / 2 = 235 px against a corner distance of hypot(384, 367) = 531.
constexpr float screenCornerDistance = 2.26f;

/** How much of the glow's net has emerged from behind the sphere at a given
 *  distance from the centre.
 *
 *  Zero on and inside the rim, rising to one over `rise` beyond it, so the
 *  filaments look like they come out from behind the sphere rather than
 *  sprouting from its edge. */
float glowEmergence (float distanceFromCentre, float rise);

/** Point in the noise domain a place on the display maps to.
 *
 *  Built from the direction vector rather than from an azimuth angle: an angle
 *  wraps, and the wrap put a visible seam due west where the filaments failed
 *  to meet. `twist` sets how much detail runs around the circle, `scale` how
 *  much runs along the radius. */
struct NetDomainPoint
{
  float x, y, z;
};

NetDomainPoint netDomainPoint (float x, float y, float radial, float twist,
                               float scale);

/** Arc of the sphere's rim a beam covers, in degrees.
 *
 *  With four speakers, 90 degrees each is what closes the circle and makes the
 *  net look like it leaves the loudspeakers rather than appearing at the rim.
 *  `angleDegrees` is the beam's angle off its own axis. */
float beamRimCoverageDegrees (float angleDegrees, float speakerRadius);

/** How far a beam's sample point is rotated about the sphere centre, in
 *  radians, at a given distance from that centre.
 *
 *  Zero at the horn's mouth and growing to `curl` by the time it reaches the
 *  sphere, so the beam leaves straight and wraps into the sphere's own turn on
 *  the way in — a straight cone reads as a foreign object next to filaments
 *  that curl. Held at both ends: there is no beam behind the mouth, and none
 *  inside the sphere. */
float beamCurlAngle (float distanceFromCentre, float mouthRadius, float curl);

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
