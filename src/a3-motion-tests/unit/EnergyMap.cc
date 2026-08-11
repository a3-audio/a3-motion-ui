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

// ── beams wrapping the sphere ───────────────────────────────────────────
//
// The beams are no longer cones aimed at the centre, curled or otherwise. They
// are bands in the annulus between the horn's mouth and the sphere: narrow
// where they leave the speaker, opening to a quarter of the way round by the
// time they reach the sphere, so the four of them close the circle and the
// sphere ends up enclosed. Their centre line wanders with fractal noise, which
// is what makes them read as roots rather than as geometry.

TEST (BeamWrap, NarrowWhereItLeavesTheHorn)
{
  auto constexpr mouthRadius = 1.35f;

  EXPECT_NEAR (beamWrapHalfAngle (mouthRadius, mouthRadius, 6.f, 45.f), 6.f,
               0.01f);
}

TEST (BeamWrap, CoversItsQuarterWhereItMeetsTheSphere)
{
  auto constexpr mouthRadius = 1.35f;

  EXPECT_NEAR (beamWrapHalfAngle (1.f, mouthRadius, 6.f, 45.f), 45.f, 0.01f);
}

// Four of them, both sides each, is the full circle — at 45 degrees the
// quarters meet exactly.
TEST (BeamWrap, FourQuartersMeetAtFortyFive)
{
  auto constexpr mouthRadius = 1.35f;
  auto const halfAngle = beamWrapHalfAngle (1.f, mouthRadius, 6.f, 45.f);

  EXPECT_NEAR (4.f * 2.f * halfAngle, 360.f, 0.1f);
}

TEST (BeamWrap, OpensAllTheWayIn)
{
  auto constexpr mouthRadius = 1.35f;

  EXPECT_LT (beamWrapHalfAngle (1.3f, mouthRadius, 6.f, 45.f),
             beamWrapHalfAngle (1.15f, mouthRadius, 6.f, 45.f));
  EXPECT_LT (beamWrapHalfAngle (1.15f, mouthRadius, 6.f, 45.f),
             beamWrapHalfAngle (1.02f, mouthRadius, 6.f, 45.f));
}

// Outside the annulus there is no band: nothing behind the mouth, nothing
// inside the sphere.
TEST (BeamWrap, HeldAtBothEnds)
{
  auto constexpr mouthRadius = 1.35f;

  EXPECT_NEAR (beamWrapHalfAngle (1.6f, mouthRadius, 6.f, 45.f), 6.f, 0.01f);
  EXPECT_NEAR (beamWrapHalfAngle (0.7f, mouthRadius, 6.f, 45.f), 45.f, 0.01f);
}

TEST (BeamWrap, ShippedConfigWrapsAQuarterEach)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("wrapAngle"));
  ASSERT_TRUE (speakerLight.hasProperty ("wander"));

  // Meeting exactly is not enough in practice: the edges are soft and frayed,
  // so two bands that only touch leave a thin seam where both have faded out.
  // Measured just outside the rim at 72 directions, gaps in the ring went
  // 7 at 45 degrees, 2 at 55, none at 65 — hence overlap rather than 45.
  EXPECT_GE (static_cast<float> (speakerLight["wrapAngle"]), 45.f)
      << "below a quarter each the four cannot enclose the sphere at all";
  EXPECT_GT (static_cast<float> (speakerLight["wander"]), 0.f)
      << "without wander the bands are geometry again, not roots";
}


// ── the band never quite lets go ────────────────────────────────────────
//
// A silent speaker used to leave its quarter dark, which opened the ring. The
// floor that fixed that was absolute, so the ring also stood there with no
// signal at all. It is relative to the loudest band instead: silence
// everywhere means nothing, one quiet speaker among loud ones still holds its
// quarter.

TEST (BeamBand, SilenceEverywhereLeavesNothing)
{
  EXPECT_FLOAT_EQ (beamBandLevel (0.f, 0.4f, 0.f), 0.f);
}

TEST (BeamBand, AQuietSpeakerAmongLoudOnesKeepsItsQuarter)
{
  EXPECT_FLOAT_EQ (beamBandLevel (0.f, 0.4f, 0.8f), 0.32f);
}

TEST (BeamBand, ALoudSpeakerKeepsItsOwnLevel)
{
  EXPECT_FLOAT_EQ (beamBandLevel (0.8f, 0.4f, 0.8f), 0.8f);
}

TEST (BeamBand, LouderStillMeansBrighter)
{
  EXPECT_LT (beamBandLevel (0.5f, 0.4f, 1.f), beamBandLevel (0.9f, 0.4f, 1.f));
}

TEST (BeamBand, ShippedConfigKeepsTheSphereEnclosed)
{
  auto const file = juce::File (A3_CONFIG_JSON_PATH);
  ASSERT_TRUE (file.existsAsFile ());

  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("levelFloor"));

  auto const floor = static_cast<float> (speakerLight["levelFloor"]);
  EXPECT_GT (floor, 0.f) << "at zero a silent speaker opens the ring again";
  EXPECT_LT (floor, 0.8f) << "too high and the band stops saying anything";
}

// The glow lives in the same annulus, so where the band is, the band is what
// you see — it covers the glow rather than adding to it.
TEST (BeamBand, BandHidesTheGlowBehindIt)
{
  EXPECT_FLOAT_EQ (glowVisibility (0.f, 2.f), 1.f);
  EXPECT_FLOAT_EQ (glowVisibility (1.f, 2.f), 0.f);
  EXPECT_LT (glowVisibility (0.3f, 2.f), 1.f);
  EXPECT_GT (glowVisibility (0.3f, 2.f), 0.f);
}

// ── and it frays at both ends ───────────────────────────────────────────
//
// The band used to stop dead at the mouth and at the rim. It bleeds past both
// now, so it runs into the glow's filaments outside and the net's inside
// instead of sitting between them as a separate object.

TEST (BeamBand, FullStrengthInsideTheAnnulus)
{
  auto constexpr mouthRadius = 1.35f;

  EXPECT_FLOAT_EQ (beamRadialWindow (1.15f, mouthRadius, 0.2f), 1.f);
}

TEST (BeamBand, ReachesOutIntoTheGlow)
{
  auto constexpr mouthRadius = 1.35f;
  auto constexpr bleed = 0.2f;

  EXPECT_GT (beamRadialWindow (mouthRadius + bleed * 0.4f, mouthRadius, bleed),
             0.f);
  EXPECT_FLOAT_EQ (beamRadialWindow (mouthRadius + bleed, mouthRadius, bleed),
                   0.f);
}

TEST (BeamBand, ReachesInIntoTheNet)
{
  auto constexpr mouthRadius = 1.35f;
  auto constexpr bleed = 0.2f;

  EXPECT_GT (beamRadialWindow (1.f - bleed * 0.4f, mouthRadius, bleed), 0.f);
  EXPECT_FLOAT_EQ (beamRadialWindow (1.f - bleed, mouthRadius, bleed), 0.f);
}

}
