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

#include <ShippedSkin.hh>

#include <a3-motion-ui/components/EnergyMap.hh>
#include <a3-motion-ui/components/SphereProjection.hh>
#include <a3-motion-ui/components/SpeakerLightScaling.hh>

#include <algorithm>
#include <cmath>
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

// The net is painted on the room, not on the display.
//
// It used to normalise the screen coordinate, which nails the whole weave to
// the glass: turn the camera and the ball rotates under a pattern that stays
// where it was. The maintainer saw it -- "die daten vom energyvisualizer
// ziehen nicht mit wenn man die sphäre kippt" -- and it is the same fault the
// graticule had before it was rebuilt in the room's terms.
TEST (EnergyMap, TheNetsBearingIsTheRoomsNotTheScreens)
{
  // What a pixel stands for at the identity camera: screenToDirection puts
  // the screen's { x, y } at { y, -x, up }.
  auto const seen = [] (float x, float y) {
    auto const r = std::min (std::hypot (x, y), 1.f);
    return Pos::fromCartesian (y, -x, std::sqrt (std::max (0.f, 1.f - r * r)));
  };

  // Untilted, the room bearing has to be the screen bearing exactly, or every
  // device nobody has tilted gets a different picture than it had.
  for (auto const &p : { std::pair<float, float>{ 0.6f, 0.f },
                         { 0.f, 0.6f },
                         { -0.4f, 0.4f },
                         { 0.3f, -0.7f } })
    {
      auto const bearing = netBearingForDirection (seen (p.first, p.second));
      auto const length = std::hypot (p.first, p.second);
      EXPECT_NEAR (bearing.x, p.first / length, 1e-5f);
      EXPECT_NEAR (bearing.y, p.second / length, 1e-5f);
    }
}

TEST (EnergyMap, WalkingRoundTheRoomCarriesTheNetWithIt)
{
  // The point of the whole change: the same place on the *ball* keeps the same
  // filament as the eye walks round it.
  auto const seen = [] (float x, float y) {
    auto const r = std::min (std::hypot (x, y), 1.f);
    return Pos::fromCartesian (y, -x, std::sqrt (std::max (0.f, 1.f - r * r)));
  };

  SphereCamera camera;
  camera.turn = 0.7f;

  auto const straightOn = netBearingForDirection (seen (0.5f, 0.f));
  auto const walked = netBearingForDirection (
      asSeenFromInverse (seen (0.5f, 0.f), camera));

  // It moved -- a screen-locked net would have given the same answer.
  auto const moved = std::hypot (walked.x - straightOn.x,
                                 walked.y - straightOn.y);
  EXPECT_GT (moved, 0.1f);

  // And it moved by the angle the eye walked, not by some other amount.
  auto const angle = [] (NetBearing const &b) {
    return std::atan2 (b.y, b.x);
  };
  auto turned = angle (walked) - angle (straightOn);
  while (turned > juce::MathConstants<float>::pi)
    turned -= juce::MathConstants<float>::twoPi;
  while (turned < -juce::MathConstants<float>::pi)
    turned += juce::MathConstants<float>::twoPi;

  EXPECT_NEAR (std::abs (turned), camera.turn, 0.02f);
}

// ── The beams as things in the room ──────────────────────────────────────
//
// The bands were a ring segment on the glass: a screen azimuth about the
// centre of the display, between the sphere's rim and a shared mouth radius.
// That is right only while every cabinet stands on the rim, which is true at
// exactly one camera. Lean the room over and the speakers come in off it while
// their bands stay welded to the ring — following a walk but not a lean.

namespace
{

// What a pixel at (x, y) stands for, in the room, under `camera`.
Pos
pixelDirection (float x, float y, SphereCamera const &camera)
{
  auto const r = std::min (std::hypot (x, y), 1.f);
  auto const seen
      = Pos::fromCartesian (y, -x, std::sqrt (std::max (0.f, 1.f - r * r)));
  return asSeenFromInverse (seen, camera);
}

} // namespace

TEST (BeamSpread, IsZeroOnTheSpeakersOwnBearing)
{
  auto const dir = Pos::fromCartesian (0.6f, 0.6f, -0.4f);

  EXPECT_NEAR (beamSpreadAngle (dir, dir), 0.f, 1e-4f);
}

TEST (BeamSpread, ReachesPiAtTheFarSideOfTheRoom)
{
  auto const here = Pos::fromCartesian (0.f, 0.f, 1.f);
  auto const there = Pos::fromCartesian (0.f, 0.f, -1.f);

  EXPECT_NEAR (beamSpreadAngle (here, there),
               juce::MathConstants<float>::pi, 1e-3f);
}

TEST (BeamSpread, DoesNotCareHowLongTheVectorsAre)
{
  auto const a = Pos::fromCartesian (1.f, 0.f, 0.f);
  auto const b = Pos::fromCartesian (0.f, 3.f, 0.f);

  EXPECT_NEAR (beamSpreadAngle (a, b), juce::MathConstants<float>::halfPi,
               1e-3f);
}

TEST (BeamSpread, TurnsWithTheRoom)
{
  // The whole point. A place on the ball and a speaker are both in the room;
  // walking round them and leaning over them must leave the angle between
  // them alone. A screen azimuth would not: it is measured about the middle of
  // the display, which is not a place in the room at all.
  SphereCamera camera;
  camera.turn = 0.7f;
  camera.pitch = 0.5f;

  auto const speaker = Pos::fromCartesian (0.65f, 0.65f, -0.39f);
  auto const straightOn = beamSpreadAngle (pixelDirection (0.4f, 0.2f, {}),
                                           speaker);
  auto const leaned
      = beamSpreadAngle (pixelDirection (0.4f, 0.2f, camera),
                         asSeenFromInverse (speaker, camera));

  EXPECT_NEAR (straightOn, leaned, 0.01f);
}

TEST (BeamSpread, MatchesTheScreenAzimuthAtTheIdentityCamera)
{
  // A device nobody has tilted must draw the picture it always drew. On the
  // rim both readings are the same angle: the old one measured it on the
  // screen, this one measures it in the room.
  auto const bearingOf = [] (float degrees) {
    auto const rad = degrees * juce::MathConstants<float>::pi / 180.f;
    return pixelDirection (std::cos (rad), std::sin (rad), {});
  };

  for (auto const separation : { 0.f, 30.f, 90.f, 150.f })
    EXPECT_NEAR (beamSpreadAngle (bearingOf (45.f + separation),
                                  bearingOf (45.f)),
                 separation * juce::MathConstants<float>::pi / 180.f, 1e-3f)
        << "at " << separation << " degrees apart";
}

TEST (BeamSpread, PartsCompanyWithTheScreenAzimuthUnderALean)
{
  // And the other half of that: it has to actually differ once the room is
  // leaned over, or the fix would change nothing.
  SphereCamera camera;
  camera.pitch = 0.8f;

  auto const speakerRoom = Pos::fromCartesian (0.65f, 0.65f, -0.39f);
  auto const speakerSeen = asSeenFrom (speakerRoom, camera);
  auto const pixel = pixelDirection (0.9f, 0.1f, {});

  // What the flat annulus would have said: both flattened onto the screen.
  auto const screenAngle = [] (Pos const &p) {
    return std::atan2 (p.x (), -p.y ());
  };
  auto flat = screenAngle (asSeenFrom (pixel, {})) - screenAngle (speakerSeen);
  while (flat > juce::MathConstants<float>::pi)
    flat -= juce::MathConstants<float>::twoPi;
  while (flat < -juce::MathConstants<float>::pi)
    flat += juce::MathConstants<float>::twoPi;

  auto const inTheRoom = beamSpreadAngle (asSeenFrom (pixel, {}), speakerSeen);

  EXPECT_GT (std::abs (std::abs (flat) - inTheRoom), 0.1f);
}

TEST (BeamAlive, ShowsNothingInASilentRoom)
{
  // What the maintainer saw on 2026-09-16: "warum sehe ich die schlieren aus
  // den Speakern obwohl kein sound anliegt?" Once the level drove thickness
  // instead of brightness, level zero still drew a bolt — thin, and just as
  // bright as a thick one.
  EXPECT_FLOAT_EQ (beamAliveness (0.f, 0.02f), 0.f);
}

TEST (BeamAlive, IsFullyOpenOnceAnythingIsPlaying)
{
  EXPECT_FLOAT_EQ (beamAliveness (0.02f, 0.02f), 1.f);
  EXPECT_FLOAT_EQ (beamAliveness (0.5f, 0.02f), 1.f);
}

TEST (BeamAlive, OpensWithoutAStep)
{
  auto previous = beamAliveness (0.f, 0.02f);
  for (auto level = 0.001f; level <= 0.03f; level += 0.001f)
    {
      auto const here = beamAliveness (level, 0.02f);
      EXPECT_GE (here, previous) << "fell at " << level;
      EXPECT_LT (here - previous, 0.2f) << "stepped at " << level;
      previous = here;
    }
}

TEST (BeamAlive, KeepsAQuietSpeakerBesideALoudOne)
{
  // It is the loudest of the four that decides, not each speaker's own level.
  // A quiet speaker in a room where something is playing is part of the
  // picture and keeps its hairline; only silence everywhere takes it away.
  auto const loudest = 0.3f;
  EXPECT_FLOAT_EQ (beamAliveness (loudest, 0.02f), 1.f);
}

TEST (BoltWidth, RunsFullAtFullLevel)
{
  EXPECT_FLOAT_EQ (boltWidthAtLevel (0.9f, 1.f, 0.35f), 0.9f);
}

TEST (BoltWidth, StaysAHairlineRatherThanGoingOutWhenTheSpeakerIsSilent)
{
  // The point of the change. A band whose brightness came off the level was
  // invisible at the levels it spends most of its time at.
  EXPECT_FLOAT_EQ (boltWidthAtLevel (0.9f, 0.f, 0.35f), 0.9f * 0.35f);
  EXPECT_GT (boltWidthAtLevel (0.9f, 0.f, 0.35f), 0.f);
}

TEST (BoltWidth, ThickensAllTheWayUpWithoutAStep)
{
  auto previous = boltWidthAtLevel (0.9f, 0.f, 0.35f);
  for (auto level = 0.05f; level <= 1.f; level += 0.05f)
    {
      auto const here = boltWidthAtLevel (0.9f, level, 0.35f);
      EXPECT_GT (here, previous) << "at level " << level;
      EXPECT_LT (here - previous, 0.9f * 0.2f) << "stepped at " << level;
      previous = here;
    }
}

TEST (BoltWidth, HoldsAtTheEndsRatherThanRunningPastThem)
{
  EXPECT_FLOAT_EQ (boltWidthAtLevel (0.9f, 2.5f, 0.35f), 0.9f);
  EXPECT_FLOAT_EQ (boltWidthAtLevel (0.9f, -1.f, 0.35f), 0.9f * 0.35f);
}

TEST (BeamBoltSeed, StaysWithTheSpeakerWhereverTheEyeGoes)
{
  // A bolt belongs to a loudspeaker, not to a place on the glass. Read off the
  // screen the seed moved with the camera, so every speaker was dealt a fresh
  // set of bolts on every frame of a walk.
  auto const room = Pos::fromCartesian (0.65f, 0.65f, -0.39f);
  auto const still = beamBoltSeed (room);

  for (auto const turn : { 0.3f, 1.2f, 2.9f })
    for (auto const pitch : { 0.f, 0.5f, 1.1f })
      {
        SphereCamera camera;
        camera.turn = turn;
        camera.pitch = pitch;
        EXPECT_FLOAT_EQ (beamBoltSeed (room), still)
            << "turn " << turn << " pitch " << pitch;
      }
}

TEST (BeamBoltSeed, GivesTheFourSpeakersFourDifferentSeeds)
{
  constexpr float k = 0.70710678f;
  std::vector<float> seeds;
  for (auto const &b : { std::pair{ k, k }, std::pair{ k, -k },
                         std::pair{ -k, -k }, std::pair{ -k, k } })
    seeds.push_back (
        beamBoltSeed (Pos::fromCartesian (b.first, b.second, -0.41f)));

  for (size_t i = 0; i < seeds.size (); ++i)
    for (size_t j = i + 1; j < seeds.size (); ++j)
      EXPECT_GT (std::abs (seeds[i] - seeds[j]), 1.f)
          << "speakers " << i << " and " << j << " share a seed";
}

TEST (BeamBoltSeed, DoesNotCareHowFarOutTheCabinetStands)
{
  // The seed that put four escaped bolts across the display carried the
  // speaker radius, because it was read off a screen bearing that is not a
  // unit vector. Scaled up it reaches noise the bolt field was never sampled
  // at, and a different set of bolts breaks out of the annulus.
  for (auto const &b : { std::pair{ 0.7071f, 0.7071f }, std::pair{ 1.f, 0.f },
                         std::pair{ 0.f, -1.f }, std::pair{ -0.6f, 0.8f } })
    {
      auto const near = Pos::fromCartesian (b.first, b.second, -0.41f);
      for (auto const out : { 1.4f, 3.f, 0.2f })
        EXPECT_FLOAT_EQ (
            beamBoltSeed (Pos::fromCartesian (b.first * out, b.second * out,
                                              -0.41f * out)),
            beamBoltSeed (near))
            << "bearing (" << b.first << ", " << b.second << ") at " << out;
    }
}

TEST (BeamMouth, SitsAtTheCabinetsOwnScreenRadius)
{
  // On the rim, straight on: the mouth is the cabinet pulled in by the horn's
  // offset, which is what the shared radius used to say for all four.
  auto const onTheRim = Pos::fromCartesian (1.4f, 0.f, 0.f);

  EXPECT_NEAR (beamMouthRadiusSeen (onTheRim), 1.4f - speakerMouthOffset,
               1e-4f);
}

TEST (BeamMouth, FollowsACabinetInUnderALean)
{
  // Leaned over, a cabinet's screen radius shrinks, and its mouth has to come
  // with it or the band starts somewhere the speaker is not.
  SphereCamera camera;
  camera.pitch = 0.9f;

  auto const room = Pos::fromCartesian (1.4f, 0.f, 0.f);
  auto const leaned = beamMouthRadiusSeen (asSeenFrom (room, camera));

  EXPECT_LT (leaned, beamMouthRadiusSeen (room));
}

TEST (BeamMouth, NeverFallsInsideTheSphere)
{
  // A cabinet crossing the silhouette would ask for an annulus with no room
  // in it, and the band's own arithmetic divides by that span.
  auto const overTheCentre = Pos::fromCartesian (0.03f, 0.f, 1.4f);

  EXPECT_GT (beamMouthRadiusSeen (overTheCentre), 1.f);
}

TEST (BeamMouth, LeavesABandRoomToBeSeenWhenTheCabinetComesInOverTheBall)
{
  // Which of the two silences a band matters. A cabinet leaned in over the
  // silhouette but still in front of the ball is in plain sight, and its band
  // should be too — so the clamp has to leave an annulus a band can live in,
  // and let beamDepthVisibility() be the one thing that takes a band away.
  auto const overTheBall = Pos::fromCartesian (0.4f, 0.f, 1.34f);

  EXPECT_NEAR (beamDepthVisibility (overTheBall, beamDepthSoftness), 1.f,
               1e-3f);

  // As much room as a cabinet standing on the rim gets, near enough: a band
  // given a tenth of that is a band nobody can see.
  auto const onTheRim = beamMouthRadiusSeen (Pos::fromCartesian (1.28f, 0.f,
                                                                 -0.57f))
                        - 1.f;
  EXPECT_GT (beamMouthRadiusSeen (overTheBall) - 1.f, onTheRim * 0.5f);
}

TEST (BeamDepth, ShowsACabinetStandingInFront)
{
  auto const inFront = Pos::fromCartesian (0.3f, 0.f, 1.37f);

  EXPECT_NEAR (beamDepthVisibility (inFront, 0.35f), 1.f, 1e-3f);
}

TEST (BeamDepth, ShowsACabinetOutBesideTheSphere)
{
  // Outside the silhouette there is nothing to hide behind, whichever side of
  // the ball it is on.
  auto const beside = Pos::fromCartesian (1.28f, 0.f, -0.57f);

  EXPECT_NEAR (beamDepthVisibility (beside, 0.35f), 1.f, 1e-3f);
}

TEST (BeamDepth, HidesACabinetGoneRoundTheBack)
{
  auto const behind = Pos::fromCartesian (0.2f, 0.f, -1.38f);

  EXPECT_LT (beamDepthVisibility (behind, 0.35f), 0.05f);
}

TEST (BeamDepth, PassesThroughTheEdgeWithoutAStep)
{
  // Walked across the silhouette's edge behind the ball, it has to arrive
  // rather than snap — a band blinking out as a speaker crosses the rim is
  // the kind of thing that reads as a bug in the drawing.
  auto previous = beamDepthVisibility (
      Pos::fromCartesian (2.0f, 0.f, -1.0f), 0.35f);

  for (auto x = 2.0f; x >= 0.f; x -= 0.02f)
    {
      auto const here
          = beamDepthVisibility (Pos::fromCartesian (x, 0.f, -1.0f), 0.35f);
      EXPECT_LT (std::abs (here - previous), 0.15f) << "stepped at x = " << x;
      previous = here;
    }
}

TEST (EnergyMap, ABearingStraightUpIsNotAnError)
{
  // The middle of the disc stands for straight up, where every bearing is the
  // same place. It has to come back as something finite rather than as a
  // division by nothing.
  auto const bearing
      = netBearingForDirection (Pos::fromCartesian (0.f, 0.f, 1.f));
  EXPECT_TRUE (std::isfinite (bearing.x));
  EXPECT_TRUE (std::isfinite (bearing.y));
}

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
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
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
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
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


// The band and the glow are one continuous piece of weather around the sphere,
// so they share a colour. They were told apart by colour once, back when it
// was unclear which element was lighting what — that is no longer the job.
TEST (BeamBand, BandAndGlowShareTheirColour)
{
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
  auto const &band = parsed["speakerLight"];
  auto const &glow = parsed["backgroundGlow"];

  for (auto const *channel : { "r", "g", "b" })
    EXPECT_EQ (static_cast<int> (band[channel]),
               static_cast<int> (glow[channel]))
        << "channel " << channel;
}


// ── bolts, not fields ───────────────────────────────────────────────────
//
// Ridged noise gives a soft field however hard you sharpen it — that is why
// the band still read as fractal. A bolt is a path with a hard bright core and
// a glow that trails off without ever quite reaching zero, which is what makes
// lightning look like lightning against a sky.

TEST (Bolts, TheCoreIsFullBrightness)
{
  EXPECT_FLOAT_EQ (boltFalloff (0.f, 0.05f), 1.f);
}

TEST (Bolts, HalfBrightnessAtOneWidthOut)
{
  EXPECT_FLOAT_EQ (boltFalloff (0.05f, 0.05f), 0.5f);
}

// The tail is the point: a smoothstep edge stops dead, a bolt keeps glowing.
// That trailing glow is what ties the band into the net inside and the glow
// outside instead of ending at a line.
TEST (Bolts, TheGlowTrailsOffWithoutEnding)
{
  auto const far = boltFalloff (1.f, 0.05f);

  EXPECT_GT (far, 0.f);
  EXPECT_LT (far, 0.1f);
}

TEST (Bolts, FallsOffMonotonically)
{
  EXPECT_GT (boltFalloff (0.1f, 0.05f), boltFalloff (0.2f, 0.05f));
}

// A bolt strikes and is gone. Without a duty cycle they all sit there at once
// and the whole thing is a texture again.
TEST (Bolts, MostOfTheTimeThereIsNoStrike)
{
  EXPECT_FLOAT_EQ (boltStrike (0.4f, 0.6f), 0.f);
  EXPECT_FLOAT_EQ (boltStrike (0.6f, 0.6f), 0.f);
  EXPECT_GT (boltStrike (1.f, 0.6f), 0.9f);
}

TEST (Bolts, ShippedConfigStrikesRatherThanGlows)
{
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("boltWidth"));
  ASSERT_TRUE (speakerLight.hasProperty ("boltDuty"));
  ASSERT_TRUE (speakerLight.hasProperty ("boltCore"));

  auto const duty = static_cast<float> (speakerLight["boltDuty"]);
  EXPECT_GT (duty, 0.f) << "at zero every bolt is always on";
  EXPECT_LT (duty, 1.f) << "at one none of them ever strike";

  EXPECT_GT (static_cast<float> (speakerLight["boltCore"]), 0.f)
      << "the core is what makes it read as lightning rather than as colour";
}


// Some bolts stay in the annulus and some break out towards the edge of the
// screen, which is what stops the band reading as a ring with a hard outer
// limit. And there have to be enough of them to look like weather.
TEST (Bolts, ShippedConfigLetsSomeBoltsBreakOut)
{
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("boltReach"));
  ASSERT_TRUE (speakerLight.hasProperty ("boltCount"));

  auto const mouthRadius = speakerMouthRadius (
      static_cast<float> (speakerLight["speakerRadius"]));

  EXPECT_GT (static_cast<float> (speakerLight["boltReach"]), mouthRadius)
      << "an escaping bolt has to get past the mouth to escape anything";
  EXPECT_GT (static_cast<float> (speakerLight["boltCount"]), 3.f);
}

// The subwoofer's glow is the slowest thing on screen — it is the room, not an
// event, so it has to outlast the bolts striking in front of it.
TEST (GlowNet, ShippedGlowFadesSlowerThanTheBolts)
{
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";

  ASSERT_TRUE (parsed["backgroundGlow"].hasProperty ("decay"));
  ASSERT_TRUE (parsed["backgroundGlow"].hasProperty ("attack"));

  auto const glowDecay = static_cast<float> (parsed["backgroundGlow"]["decay"]);

  EXPECT_GT (glowDecay, static_cast<float> (parsed["backgroundGlow"]["attack"]))
      << "it has to fade slower than it rises, or it flickers";
  EXPECT_GT (glowDecay, static_cast<float> (parsed["speakerLight"]["decay"]))
      << "the room outlasts the events in front of it";
}


// A branch shares its trunk until it forks, then goes its own way — that is
// what makes a bolt look like lightning rather than like several parallel
// lines that happen to wobble.

TEST (Bolts, ABranchFollowsItsTrunkUntilItForks)
{
  EXPECT_FLOAT_EQ (boltBranchOffset (1.1f, 1.3f, 0.5f), 0.f);
  EXPECT_FLOAT_EQ (boltBranchOffset (1.3f, 1.3f, 0.5f), 0.f);
}

TEST (Bolts, ABranchLeavesAfterTheFork)
{
  EXPECT_GT (boltBranchOffset (1.6f, 1.3f, 0.5f), 0.f);
}

TEST (Bolts, ABranchKeepsLeaving)
{
  EXPECT_LT (boltBranchOffset (1.5f, 1.3f, 0.5f),
             boltBranchOffset (1.9f, 1.3f, 0.5f));
}

TEST (Bolts, ShippedConfigBranchesAndRunsThin)
{
  auto const parsed = shippedSkin ();
  ASSERT_FALSE (parsed.isVoid ()) << "no skin to check";
  auto const &speakerLight = parsed["speakerLight"];

  ASSERT_TRUE (speakerLight.hasProperty ("boltBranch"));

  EXPECT_GT (static_cast<float> (speakerLight["boltBranch"]), 0.f)
      << "at zero every branch sits on top of its trunk";
  EXPECT_LT (static_cast<float> (speakerLight["boltWidth"]), 0.7f)
      << "a thick bolt reads as a band again";
}


// The sphere used to freeze while the menu was open — deliberate, to save the
// RPi4 some work. The rig runs on an Intel NUC now and the effects are meant to
// carry on, but the option has to survive for a possible RPi later.
TEST (MenuRendering, ShippedConfigKeepsTheSphereRunningWhileTheMenuIsOpen)
{
  // Operational, not a look — stays in config.json while the skin values move.
  auto const parsed = juce::JSON::parse (
      juce::File (A3_CONFIG_JSON_PATH).loadFileAsString ());
  auto const &ui = parsed["ui"];

  ASSERT_TRUE (ui.hasProperty ("pauseRenderingInMenu"))
      << "the switch has to exist, or the behaviour is baked in again";
  EXPECT_FALSE (static_cast<bool> (ui["pauseRenderingInMenu"]));
}

}
