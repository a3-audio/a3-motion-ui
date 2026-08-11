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
#include <a3-motion-ui/components/SpeakerLightScaling.hh>

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


// ── the net's noise domain ──────────────────────────────────────────────
//
// Feeding the noise a plain azimuth angle puts a seam where atan2 wraps, due
// west on the horizontal, and the filaments visibly fail to meet across it.
// Building the domain from the direction vector instead closes the circle by
// construction.

TEST (EnergyNet, DomainHasNoSeamInTheWest)
{
  auto constexpr radial = 0.8f;
  auto constexpr twist = 9.f;
  auto constexpr scale = 9.f;

  // Just above and just below the horizontal on the west side — the two sides
  // of the old seam.
  auto const above = netDomainPoint (-1.f, 0.002f, radial, twist, scale);
  auto const below = netDomainPoint (-1.f, -0.002f, radial, twist, scale);

  EXPECT_NEAR (above.x, below.x, 0.05f);
  EXPECT_NEAR (above.y, below.y, 0.05f);
  EXPECT_NEAR (above.z, below.z, 0.05f);
}

TEST (EnergyNet, DomainStillSeparatesDifferentDirections)
{
  auto constexpr radial = 0.8f;

  auto const west = netDomainPoint (-1.f, 0.f, radial, 9.f, 9.f);
  auto const east = netDomainPoint (1.f, 0.f, radial, 9.f, 9.f);

  EXPECT_GT (std::abs (west.x - east.x) + std::abs (west.y - east.y), 1.f);
}

TEST (EnergyNet, DomainMovesWithTheRadius)
{
  auto const near = netDomainPoint (0.f, 1.f, 0.4f, 9.f, 9.f);
  auto const far = netDomainPoint (0.f, 1.f, 0.9f, 9.f, 9.f);

  EXPECT_GT (std::abs (far.z - near.z), 1.f);
}

// ── how much rim a beam covers ──────────────────────────────────────────

// The quarter-of-the-rim criterion is gone: the beams are being reworked from
// straight cones into something that wraps the sphere with the same fractal
// character as everything else, and a cone's rim coverage says nothing about
// that. What is left here is the pair of invariants that survive the change.
TEST (SpeakerBeamReach, ShippedBeamsDoNotNarrowWithLevel)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  auto const quiet = static_cast<float> (speakerLight["beamAngleQuiet"]);
  auto const loud = static_cast<float> (speakerLight["beamAngleLoud"]);

  EXPECT_GT (quiet, 0.f);
  EXPECT_LE (quiet, loud);
}

TEST (SpeakerBeamReach, WiderBeamsCoverMoreRim)
{
  EXPECT_GT (beamRimCoverageDegrees (60.f, 1.35f),
             beamRimCoverageDegrees (30.f, 1.35f));
}




// ── the glow's outward net ──────────────────────────────────────────────
//
// The sphere glow used to be a smooth halo hugging the rim. It is filaments
// now, running the other way from the inner net: out from behind the sphere
// towards the edge of the screen, so the picture reads as arrival on the
// inside and spread on the outside.

TEST (GlowNet, FilamentsTravelOutwardsWhenTheFlowIsNegative)
{
  auto constexpr filament = 1.0f;
  auto constexpr flow = -0.2f;

  EXPECT_GT (netFilamentRadius (filament, 1.f, flow),
             netFilamentRadius (filament, 0.f, flow));
}

TEST (GlowNet, NothingEmergesInsideTheSphere)
{
  EXPECT_FLOAT_EQ (glowEmergence (0.9f, 0.25f), 0.f);
  EXPECT_FLOAT_EQ (glowEmergence (1.f, 0.25f), 0.f);
}

// The filaments have to look like they come out from behind the sphere rather
// than sprouting from its edge, which means nothing at the rim itself.
TEST (GlowNet, FilamentsAppearBehindTheRimNotOnIt)
{
  auto constexpr rise = 0.25f;

  EXPECT_LT (glowEmergence (1.02f, rise), 0.1f);
  EXPECT_GT (glowEmergence (1.f + rise, rise), 0.9f);
}

// The screen corner sits about 2.26 sphere radii out: the component is
// 768 x ~734 px with the sphere at reduceFactorCircle 0.64, so a radius of
// 734 * 0.64 / 2 = 235 px against a corner distance of hypot(384, 367) = 531.
// A filament domain that stops short of that freezes exactly where the
// filaments are supposed to be heading.
TEST (GlowNet, ShippedConfigReachesTheScreenCorner)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &glow = parsed["sphereGlow"];

  ASSERT_TRUE (glow.hasProperty ("reach"));
  EXPECT_GE (static_cast<float> (glow["reach"]), screenCornerDistance);
}

TEST (GlowNet, ShippedConfigRunsOutwards)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &glow = parsed["sphereGlow"];

  ASSERT_TRUE (glow.hasProperty ("netFlow"));
  EXPECT_LT (static_cast<float> (glow["netFlow"]), 0.f)
      << "a positive flow would run the glow inwards, against the inner net";
}

// The smooth halo is what these replace; leaving it in would just sit under
// them and put a step back at the rim.
TEST (GlowNet, ShippedConfigHasNoSmoothHaloLeft)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());

  EXPECT_FALSE (parsed["sphereGlow"].hasProperty ("falloff"));
}


// ── beams wrapping the sphere ───────────────────────────────────────────
//
// A straight cone reads as a foreign object next to filaments that curl. The
// beam's sample point gets rotated about the sphere centre by an angle that
// grows as it approaches, so the beam leaves the horn straight and curls into
// the sphere's own turn by the time it arrives.

TEST (BeamCurl, BeamLeavesTheHornStraight)
{
  auto constexpr mouthRadius = 1.35f;
  auto constexpr curl = 0.6f;

  EXPECT_NEAR (beamCurlAngle (mouthRadius, mouthRadius, curl), 0.f, 1e-4f);
}

TEST (BeamCurl, BeamIsFullyTurnedByTheTimeItReachesTheSphere)
{
  auto constexpr mouthRadius = 1.35f;
  auto constexpr curl = 0.6f;

  EXPECT_NEAR (beamCurlAngle (1.f, mouthRadius, curl), curl, 1e-4f);
}

TEST (BeamCurl, TurnGrowsAllTheWayIn)
{
  auto constexpr mouthRadius = 1.35f;
  auto constexpr curl = 0.6f;

  EXPECT_LT (beamCurlAngle (1.3f, mouthRadius, curl),
             beamCurlAngle (1.15f, mouthRadius, curl));
  EXPECT_LT (beamCurlAngle (1.15f, mouthRadius, curl),
             beamCurlAngle (1.05f, mouthRadius, curl));
}

// Behind the mouth and inside the sphere there is no beam to turn, and letting
// the angle run on there would spin the pattern where it is not drawn.
TEST (BeamCurl, TurnStopsAtBothEnds)
{
  auto constexpr mouthRadius = 1.35f;
  auto constexpr curl = 0.6f;

  EXPECT_FLOAT_EQ (beamCurlAngle (1.6f, mouthRadius, curl), 0.f);
  EXPECT_FLOAT_EQ (beamCurlAngle (0.8f, mouthRadius, curl), curl);
}

TEST (BeamCurl, ShippedConfigCurlsTheBeams)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("curl"));
  EXPECT_GT (std::abs (static_cast<float> (speakerLight["curl"])), 0.1f)
      << "a straight beam is what this was meant to get away from";
}

}
