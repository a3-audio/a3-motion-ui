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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/EnergyMap.hh>

#include <algorithm>
#include <vector>

using namespace a3;

namespace
{

// A handful of directions standing in for the plugin's grid.
std::vector<EnergyDirection>
testGrid ()
{
  return { { 0.f, 0.f },    // front
           { 90.f, 0.f },   // left
           { 180.f, 0.f },  // back
           { -90.f, 0.f },  // right
           { 0.f, 90.f },   // top
           { 0.f, -90.f } };
}

float
texelAt (std::vector<float> const &map, float azimuth, float elevation)
{
  return map[static_cast<size_t> (energyMapTexel (azimuth, elevation))];
}

TEST (EnergyMap, EnergyLandsInTheDirectionItCameFrom)
{
  auto const grid = testGrid ();
  EnergyMapProjection projection{ grid, 20.f };

  std::vector<float> values (grid.size (), 0.f);
  values[1] = 1.f; // left

  std::vector<float> map (energyMapTexelCount, 0.f);
  projection.project (values.data (), map.data ());

  EXPECT_GT (texelAt (map, 90.f, 0.f), texelAt (map, -90.f, 0.f));
  EXPECT_GT (texelAt (map, 90.f, 0.f), texelAt (map, 180.f, 0.f));
}

// The plugin sends 9 updates a second, so the map is interpolated rather than
// stepped — but the map itself must not smear energy across the whole sphere
// to achieve that.
TEST (EnergyMap, EnergyDoesNotSmearToTheOppositeSide)
{
  auto const grid = testGrid ();
  EnergyMapProjection projection{ grid, 20.f };

  std::vector<float> values (grid.size (), 0.f);
  values[0] = 1.f; // front

  std::vector<float> map (energyMapTexelCount, 0.f);
  projection.project (values.data (), map.data ());

  EXPECT_LT (texelAt (map, 180.f, 0.f), texelAt (map, 0.f, 0.f) * 0.1f);
}

TEST (EnergyMap, TopAndBottomAreToldApart)
{
  auto const grid = testGrid ();
  EnergyMapProjection projection{ grid, 20.f };

  std::vector<float> values (grid.size (), 0.f);
  values[4] = 1.f; // top

  std::vector<float> map (energyMapTexelCount, 0.f);
  projection.project (values.data (), map.data ());

  EXPECT_GT (texelAt (map, 0.f, 85.f), texelAt (map, 0.f, -85.f));
}

// A hole in the map shows up as a dark band on the sphere, so every texel has
// to be reachable from some grid point.
TEST (EnergyMap, EveryTexelReceivesSomeEnergy)
{
  auto const grid = testGrid ();
  EnergyMapProjection projection{ grid, 45.f };

  std::vector<float> values (grid.size (), 1.f);

  std::vector<float> map (energyMapTexelCount, 0.f);
  projection.project (values.data (), map.data ());

  EXPECT_GT (*std::min_element (map.begin (), map.end ()), 0.f);
}

TEST (EnergyMap, ShippedGridHasThePluginsPointCount)
{
  auto const grid = loadEnergyGrid (
      juce::File (A3_RESOURCE_DIR).getChildFile ("EnergyVisualizerGrid.json"));

  ASSERT_EQ (grid.size (), 426u)
      << "index order has to match /EnergyVisualizer/RMS";

  // Full sphere, not just the horizon.
  auto const [lo, hi] = std::minmax_element (
      grid.begin (), grid.end (), [] (auto const &a, auto const &b) {
        return a.elevationDegrees < b.elevationDegrees;
      });
  EXPECT_LT (lo->elevationDegrees, -85.f);
  EXPECT_GT (hi->elevationDegrees, 85.f);
}


// ── screen position to direction ────────────────────────────────────────
//
// Anchored on the blob pipeline, because that is what actually decides where a
// direction appears: Position::azimuth() is atan2(y, x) in HOA coordinates and
// goes straight to /StereoEncoder/azimuth, and cartesian2DHOA2JUCE (Helpers.hh)
// puts that on screen as { -y, -x } with JUCE's y pointing down. Following that
// through leaves azimuth 0 at the top of the disc and 90 to the left.
//
// The speaker icons are *not* a reliable reference here: drawSpeakers() places
// them by raw angle without that conversion, so they do not sit where the same
// azimuth would put a blob.

TEST (EnergyMap, AzimuthZeroIsAtTheTopOfTheDisc)
{
  // Screen coordinates with y up, as the shader uses them.
  auto const direction = energyDirectionForScreen (0.f, 1.f);

  EXPECT_NEAR (direction.azimuthDegrees, 0.f, 0.5f);
  EXPECT_NEAR (direction.elevationDegrees, 0.f, 0.5f);
}

TEST (EnergyMap, AzimuthRunsAnticlockwiseFromTheTop)
{
  EXPECT_NEAR (energyDirectionForScreen (-1.f, 0.f).azimuthDegrees, 90.f, 0.5f);
  EXPECT_NEAR (energyDirectionForScreen (1.f, 0.f).azimuthDegrees, -90.f, 0.5f);
  EXPECT_NEAR (std::abs (energyDirectionForScreen (0.f, -1.f).azimuthDegrees),
               180.f, 0.5f);
}

TEST (EnergyMap, CentreOfTheDiscIsStraightUp)
{
  EXPECT_NEAR (energyDirectionForScreen (0.f, 0.f).elevationDegrees, 90.f,
               0.5f);
}

TEST (EnergyMap, HalfWayOutIsHalfWayUp)
{
  // Orthographic, so elevation is asin of the height — not linear in radius.
  auto const direction = energyDirectionForScreen (0.5f, 0.f);

  EXPECT_NEAR (direction.elevationDegrees, 60.f, 0.5f);
}


// ── the net's flow ──────────────────────────────────────────────────────
//
// The filaments start out in the speaker beams and at the rim and travel
// inwards. A sign flip here would send them the other way, which is exactly
// the difference between sound arriving and sound leaving.

TEST (EnergyNet, FilamentsTravelTowardsTheCentre)
{
  auto constexpr filament = 1.0f;
  auto constexpr flow = 0.2f;

  auto const early = netFilamentRadius (filament, 0.f, flow);
  auto const late = netFilamentRadius (filament, 1.f, flow);

  EXPECT_LT (late, early);
}

TEST (EnergyNet, FilamentsCrossTheRimOnTheWayIn)
{
  auto constexpr filament = 1.3f;
  auto constexpr flow = 0.2f;

  // Starts outside the sphere, where the beams are, and ends up inside it.
  EXPECT_GT (netFilamentRadius (filament, 0.f, flow), 1.f);
  EXPECT_LT (netFilamentRadius (filament, 2.f, flow), 1.f);
}

}
