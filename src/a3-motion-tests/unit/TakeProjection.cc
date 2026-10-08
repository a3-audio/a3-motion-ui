/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/TakeProjection.hh>
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <algorithm>
#include <cmath>
#include <utility>

using namespace a3;

namespace
{

constexpr float ticksPerBar = 128.f * 4.f; // ticksPerBeat x a four-beat bar

Pos
directionAt (float frac, float azimuth)
{
  auto const t = frac * juce::MathConstants<float>::pi;
  return Pos::fromCartesian (std::sin (t) * std::cos (azimuth),
                             std::sin (t) * std::sin (azimuth), std::cos (t));
}

float
angleBetween (Pos const &a, Pos const &b)
{
  auto const dot = a.x () * b.x () + a.y () * b.y () + a.z () * b.z ();
  return std::acos (std::clamp (dot, -1.f, 1.f));
}

/** Fingers across the whole sphere a default clip can play. */
std::vector<Pos>
fingersEverywhere ()
{
  std::vector<Pos> fingers;
  for (float frac : { 0.15f, 0.3f, 0.45f, 0.6f, 0.75f })
    for (float azimuth : { -2.8f, -1.3f, 0.f, 0.7f, 2.2f })
      fingers.push_back (directionAt (frac, azimuth));
  return fingers;
}

/** The colatitudes a clip can play, top and bottom, by running the forward
 *  map over the pad. */
std::pair<float, float>
bandOf (HeightMapSphere const &heightMap, ElevationParams const &params)
{
  auto top = 1.f;
  auto bottom = 0.f;
  for (int i = 0; i <= 40000; ++i)
    {
      auto const d = heightMap.mapTo3D (
          Pos::fromCartesian (4.f * static_cast<float> (i) / 40000.f, 0.f,
                              0.f),
          params);
      auto const frac = std::acos (std::clamp (d.z (), -1.f, 1.f))
                        / juce::MathConstants<float>::pi;
      top = std::min (top, frac);
      bottom = std::max (bottom, frac);
    }
  return { top, bottom };
}

/** The finger, written and then played: where the take is heard. */
Pos
roundTrip (HeightMapSphere const &heightMap, Pos const &finger,
           Pattern const &take)
{
  return playedPosition (heightMap,
                         writtenPosition (heightMap, finger, take), take);
}

}

// ── The plane's shaping undone ───────────────────────────────────────────

/** Squeeze then turn, and back: turn back, then unsqueeze. A squeeze is a
 *  power of two between a half and two, so it never collapses an axis and
 *  the inverse is exact everywhere. */
TEST (TakeProjection, UnshapingUndoesTheSqueezeAndTheTurn)
{
  PlaneShaping shaping;
  shaping.turns = 0.37f;
  shaping.squeezeX = -1.f;
  shaping.squeezeY = 0.6f;

  for (auto const &p : { Pos::fromCartesian (1.f, 0.f, 0.f),
                         Pos::fromCartesian (-0.3f, 0.9f, 0.f),
                         Pos::fromCartesian (0.7f, -0.7f, 0.f) })
    {
      auto const back = shapedPosition (unshapedPosition (p, shaping), shaping);
      EXPECT_NEAR (back.x (), p.x (), 1e-5f);
      EXPECT_NEAR (back.y (), p.y (), 1e-5f);
    }
}

// ── Where a take is heard is where the finger was ────────────────────────

/** Nothing turned, nothing squeezed: the take is written exactly as before
 *  this existed -- the band's own inverse and nothing else. */
TEST (TakeProjection, AClipWithoutRotateOrSqueezeIsWrittenAsBefore)
{
  HeightMapSphere heightMap;
  Pattern take;
  take.setElevationBase (0.35f);
  take.setReach (0.7f);

  for (auto const &finger : fingersEverywhere ())
    {
      auto const written = writtenPosition (heightMap, finger, take);
      auto const before = heightMap.mapTo2D (finger, take.getElevationParams ());
      EXPECT_FLOAT_EQ (written.x (), before.x ());
      EXPECT_FLOAT_EQ (written.y (), before.y ());
    }
}

TEST (TakeProjection, AStaticRotationIsUndone)
{
  HeightMapSphere heightMap;
  Pattern take;
  take.setRotate (0.3f);

  for (auto const &finger : fingersEverywhere ())
    EXPECT_LT (angleBetween (roundTrip (heightMap, finger, take), finger),
               1e-3f);
}

/** Pressed flat along one axis and stretched along the other. The finger out
 *  along the pressed axis is written twice as far out as the pad reaches, and
 *  played back under the finger -- a squeeze of a half has nowhere else to
 *  keep it. */
TEST (TakeProjection, ASqueezeIsUndone)
{
  HeightMapSphere heightMap;
  Pattern take;
  take.setSqueezeX (-1.f);
  take.setSqueezeY (0.8f);

  for (auto const &finger : fingersEverywhere ())
    EXPECT_LT (angleBetween (roundTrip (heightMap, finger, take), finger),
               1e-3f);
}

/** A spinning clip turns further every tick, so the same finger is written
 *  differently at each one -- and heard where it was at every one. */
TEST (TakeProjection, ASpinIsUndoneAtTheTickItIsHeardOn)
{
  HeightMapSphere heightMap;
  Pattern take;
  take.setRotate (0.1f);
  take.setSpin (6); // a bar a turn

  auto const finger = directionAt (0.4f, 1.f);
  Pos previous = Pos::invalid;
  for (float ticks : { 0.f, 37.f, 128.f, 300.f, 511.f, 1500.f })
    {
      setPassPhases (take, ticks, ticksPerBar);
      auto const written = writtenPosition (heightMap, finger, take);
      EXPECT_LT (angleBetween (playedPosition (heightMap, written, take),
                               finger),
                 1e-3f)
          << "at " << ticks << " ticks into the pass";
      if (previous.isValid ())
        EXPECT_GT (std::hypot (written.x () - previous.x (),
                               written.y () - previous.y ()),
                   1e-3f)
            << "the spin was not undone at " << ticks;
      previous = written;
    }
}

/** The slow sweeps run the same way: the squeezes' stretch, the swell and
 *  sway of the band, the tilt and roll. Each at its own phase. */
TEST (TakeProjection, TheSweepsAreUndoneAtTheTickTheyAreHeardOn)
{
  HeightMapSphere heightMap;
  Pattern take;
  take.setSqueezeX (0.3f);
  take.setSqueezeXLfo (5);
  take.setSqueezeYLfo (-6);
  take.setTilt (0.2f);
  take.setTiltLfo (4);
  take.setRollLfo (-5);
  take.setReachLfo (6);
  take.setElevationLfo (5);

  for (float ticks : { 0.f, 100.f, 256.f, 777.f })
    {
      setPassPhases (take, ticks, ticksPerBar);
      for (auto const &finger : fingersEverywhere ())
        {
          // The band breathes, so a finger may lie outside it at this tick:
          // then the round trip lands on the nearest playable direction, and
          // doing it again changes nothing.
          auto const heard = roundTrip (heightMap, finger, take);
          EXPECT_LT (angleBetween (roundTrip (heightMap, heard, take), heard),
                     1e-3f)
              << "at " << ticks << " ticks into the pass";
        }
    }
}

/** "Build Pulse" turned, squeezed, leant and spinning: a finger inside the
 *  band is heard where it was; one above it is held on the band's top edge in
 *  its own bearing (#66), and the turn and squeeze do not move that edge. */
TEST (TakeProjection, BandRotateAndSqueezeTogether)
{
  HeightMapSphere heightMap;
  Pattern take;
  take.setElevationBase (0.35f);
  take.setReach (0.7f);
  take.setRotate (0.3f);
  take.setSqueezeX (-0.7f);
  take.setSqueezeY (0.5f);
  take.setTilt (0.3f);
  take.setSpin (-5);
  setPassPhases (take, 200.f, ticksPerBar);

  auto const turn = spaceTurnOf (take);
  auto const [top, bottom] = bandOf (heightMap, take.getElevationParams ());
  ASSERT_GT (top, 0.15f) << "the band has to leave room above it";

  for (float share : { 0.1f, 0.5f, 0.9f })
    for (float azimuth : { -2.f, 0.5f, 2.5f })
      {
        auto const frac = top + share * (bottom - top);
        auto const finger = turnedInSpace (directionAt (frac, azimuth), turn);
        EXPECT_LT (angleBetween (roundTrip (heightMap, finger, take), finger),
                   1e-3f)
            << "inside the band at frac " << frac;
      }

  for (float azimuth : { -2.f, 0.5f, 2.5f })
    {
      auto const above = turnedInSpace (directionAt (0.1f, azimuth), turn);
      auto const heard = unturnedInSpace (roundTrip (heightMap, above, take),
                                          turn);
      EXPECT_NEAR (std::atan2 (heard.y (), heard.x ()), azimuth, 1e-3f);
      EXPECT_NEAR (std::acos (std::clamp (heard.z (), -1.f, 1.f))
                       / juce::MathConstants<float>::pi,
                   top, 2e-3f)
          << "not held on the band's top edge";
    }
}

// ── When a tick is heard ─────────────────────────────────────────────────

/** A pass starts at its first tick forwards and its last in reverse; a
 *  bounce spreads its outward leg over one tick fewer (see
 *  fractionalTickForPlayback). */
TEST (TakeProjection, TicksIntoTheFirstPassFollowTheDirection)
{
  EXPECT_FLOAT_EQ (ticksIntoFirstPass (0, 1024, PlayDirection::Forward, 512.f),
                   0.f);
  EXPECT_FLOAT_EQ (
      ticksIntoFirstPass (256, 1024, PlayDirection::Forward, 512.f), 128.f);
  EXPECT_FLOAT_EQ (
      ticksIntoFirstPass (256, 1024, PlayDirection::Reverse, 512.f), 384.f);
  EXPECT_FLOAT_EQ (
      ticksIntoFirstPass (1023, 1024, PlayDirection::Bounce, 1023.f), 1023.f);
  EXPECT_FLOAT_EQ (
      ticksIntoFirstPass (100, 1024, PlayDirection::Random, 512.f), 50.f);
}

/** The phases a pass has after n ticks are the ones playback reaches by
 *  advancing them one tick at a time from beginPass. */
TEST (TakeProjection, PassPhasesAreWhatPlaybackAccumulates)
{
  Pattern take;
  take.setSpin (6);
  take.setSqueezeXLfo (-4);
  take.setSqueezeYLfo (7);
  take.setTiltLfo (3);
  take.setRollLfo (-8);
  take.setReachLfo (5);
  take.setElevationLfo (-6);

  float spin = 0.f, sqx = 0.f, sqy = 0.f, tilt = 0.f, roll = 0.f,
        reach = 0.f, elevation = 0.f;
  for (int n = 1; n <= 1500; ++n)
    {
      spin = advanceLfoPhase (spin, 6, ticksPerBar);
      sqx = advanceLfoPhase (sqx, -4, ticksPerBar);
      sqy = advanceLfoPhase (sqy, 7, ticksPerBar);
      tilt = advanceLfoPhase (tilt, 3, ticksPerBar);
      roll = advanceLfoPhase (roll, -8, ticksPerBar);
      reach = advanceLfoPhase (reach, 5, ticksPerBar);
      elevation = advanceLfoPhase (elevation, -6, ticksPerBar);
    }

  setPassPhases (take, 1500.f, ticksPerBar);

  // On a ring: 0.9999 and 0.0001 are a hair apart.
  auto const ringDistance = [] (float a, float b) {
    auto const d = std::abs (a - b);
    return std::min (d, 1.f - d);
  };
  EXPECT_LT (ringDistance (take.getSpinPhase (), spin), 1e-3f);
  EXPECT_LT (ringDistance (take.getSqueezeXLfoPhase (), sqx), 1e-3f);
  EXPECT_LT (ringDistance (take.getSqueezeYLfoPhase (), sqy), 1e-3f);
  EXPECT_LT (ringDistance (take.getTiltLfoPhase (), tilt), 1e-3f);
  EXPECT_LT (ringDistance (take.getRollLfoPhase (), roll), 1e-3f);
  EXPECT_LT (ringDistance (take.getReachLfoPhase (), reach), 1e-3f);
  EXPECT_LT (ringDistance (take.getElevationLfoPhase (), elevation), 1e-3f);
}
