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
// Anchored on a fact from the rig: speaker 1 is drawn at the top left and sits
// at IEM azimuth -45. Everything else follows from that plus the display being
// an orthographic view of the upper hemisphere from above — the same
// projection the sphere shader's own normal already describes, which is why
// the blobs sit on the rim at the horizon rather than half way out.

TEST (EnergyMap, TopLeftIsTheDirectionOfTheFirstSpeaker)
{
  // Screen coordinates with y up, as the shader uses them.
  auto const direction = energyDirectionForScreen (-0.7071f, 0.7071f);

  EXPECT_NEAR (direction.azimuthDegrees, -45.f, 0.5f);
  EXPECT_NEAR (direction.elevationDegrees, 0.f, 0.5f);
}

TEST (EnergyMap, SpeakersRunClockwiseFromTheTopLeft)
{
  EXPECT_NEAR (energyDirectionForScreen (0.7071f, 0.7071f).azimuthDegrees,
               45.f, 0.5f);
  EXPECT_NEAR (energyDirectionForScreen (0.7071f, -0.7071f).azimuthDegrees,
               135.f, 0.5f);
  EXPECT_NEAR (std::abs (
                   energyDirectionForScreen (-0.7071f, -0.7071f).azimuthDegrees),
               135.f, 0.5f);
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

}
