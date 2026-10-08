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

#include "WaitUntil.hh"

#include <OfflineBackend.hh>

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/ClipFile.hh>
#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/SpaceTurn.hh>
#include <a3-motion-engine/TakeProjection.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>
#include <a3-motion-engine/TempoLfo.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace a3;

namespace
{

std::unique_ptr<MotionEngine>
anOfflineEngine (HeightMap &heightMap)
{
  return std::make_unique<MotionEngine> (4, heightMap, offlineBackend ());
}

float
fracOf (Pos const &pos3D)
{
  return std::acos (std::clamp (pos3D.z (), -1.f, 1.f))
         / juce::MathConstants<float>::pi;
}

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

/** "Build Pulse": the clip whose takes collapsed onto one ring (#66). */
std::shared_ptr<Pattern>
aTakeOverBuildPulse ()
{
  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->setElevationBase (0.35f);
  take->setReach (0.7f);
  return take;
}

bool
anyTickWritten (Pattern const &pattern)
{
  auto const ticks = pattern.getTicks ().positions;
  return std::any_of (ticks.begin (), ticks.end (),
                      [] (Pos const &p) { return p.isValid (); });
}

/** A clip whose whole path stands on one spot, to record over. */
std::shared_ptr<Pattern>
aClipStandingAt (Pos const &position2D, index_t ticks)
{
  auto clip = std::make_shared<Pattern> ();
  clip->setChannel (0);
  clip->resize (ticks);
  for (index_t tick = 0; tick < ticks; ++tick)
    clip->setTick (tick, position2D);
  return clip;
}

/** Where a OneShot take starts. Its stop is scheduled from the timepoint it
 *  was asked for, not from when it began, so a timepoint already past (as
 *  Measure{} is a tick or two after the engine starts) cuts the take short by
 *  that much -- the UI always asks for a downbeat still ahead. A fresh
 *  engine's clock starts at zero, so one beat in is still to come. */
Measure const aBeatFromNow{ 0, 1, 0 };

/** Whether the take plays `tick` at `direction`, through its own band. */
bool
heardAt (HeightMapSphere const &heightMap, Pattern const &take, Pos const &p,
         Pos const &direction)
{
  return p.isValid ()
         && angleBetween (heightMap.mapTo3D (p, take.getElevationParams ()),
                          direction)
                < 2e-3f;
}

bool
near2D (Pos const &a, Pos const &b)
{
  return a.isValid () && b.isValid ()
         && std::hypot (a.x () - b.x (), a.y () - b.y ()) < 1e-3f;
}

/** Where the first pass of a forward take plays its tick `tick`, worked out
 *  from the playback's own pieces -- squeeze and turn, the band, the lean --
 *  rather than through anything the recording side uses. `lapBars` is the
 *  take's playback length; the spin has turned by that share of it. */
Pos
heardOnFirstPass (HeightMapSphere const &heightMap, Pattern const &take,
                  std::size_t tick, float lapBars)
{
  auto const ticks = take.getTicks ().positions;
  auto const fraction
      = static_cast<float> (tick) / static_cast<float> (ticks.size ());
  auto const spin = take.getKnobStep (Knob::Spin);
  auto const spun
      = spin == 0 ? 0.f : fraction * lapBars * lfoCyclesPerBar (spin);

  PlaneShaping shaping;
  shaping.turns = take.getKnob (Knob::Rotate) + spun;
  shaping.squeezeX = take.getKnob (Knob::SqueezeX);
  shaping.squeezeY = take.getKnob (Knob::SqueezeY);

  return turnedInSpace (
      heightMap.mapTo3D (shapedPosition (ticks[tick], shaping),
                         take.getElevationParams ()),
      SpaceTurn{ take.getKnob (Knob::Tilt), take.getKnob (Knob::Roll) });
}

/** A take over a clip that is turned, squeezed, leant and spinning a bar a
 *  turn -- every part of the plane's shaping at once. */
std::shared_ptr<Pattern>
aTakeOverATurnedSqueezedSpinningClip ()
{
  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->setRotate (0.3f);
  take->setSqueezeX (-0.8f);
  take->setSqueezeY (0.4f);
  take->setTilt (0.25f);
  take->setSpin (6); // one bar a turn
  take->setPlaybackLength (Measure{ 1, 0, 0 });
  return take;
}

}

// ── A take records over the whole sphere (2026-10-08) ────────────────────

/** A clip's band grows south from its base, so with the base at ear height
 *  only the lower half is playable -- and the camera looks from above, so
 *  every finger landed on the equator, the sphere's outer line (maintainer,
 *  2026-10-08). Decided: while a take records, its band is the whole sphere,
 *  and the take keeps that band, so what is heard is what plays back (#66). */
TEST (TakeRecording, AFingerAboveTheClipsBandIsRecordedWhereItIs)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setTempoBPM (120.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::Loop);
  engine->setRecMode (RecMode::Touch);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->setElevationBase (0.5f);
  take->setReach (0.3f);
  engine->recordPattern (take, Measure{}, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine->isRecording (); }));

  // 45 degrees from the zenith: above the band, which starts at the equator.
  auto const finger = directionAt (0.25f, 0.4f);
  engine->setRecording3DPosition (finger);
  ASSERT_TRUE (waitUntil ([&] { return anyTickWritten (*take); }));
  juce::Thread::sleep (20);

  EXPECT_LT (angleBetween (engine->getChannelPosition (0), finger), 2e-3f)
      << "the blob was held away from the finger, at frac "
      << fracOf (engine->getChannelPosition (0));

  auto const ticks = take->getTicks ().positions;
  float worst = 0.f;
  for (std::size_t tick = 0; tick < ticks.size (); ++tick)
    if (ticks[tick].isValid ())
      worst = std::max (
          worst, angleBetween (heightMap.mapTo3D (ticks[tick],
                                                  take->getElevationParams ()),
                               finger));
  EXPECT_LT (worst, 2e-3f) << "a written tick plays back away from the finger";
}

/** The take keeps the band it was recorded in: the whole sphere, top at the
 *  north pole, nothing clipped, no sweep moving it. */
TEST (TakeRecording, ATakeKeepsTheWholeSphere)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setTempoBPM (240.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Touch);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->setElevationBase (0.5f);
  take->setReach (0.3f);
  take->setClipTop (0.2f);
  take->setClipBottom (0.1f);
  take->setKnobSetting (Knob::Sway, 3.f);
  take->setKnobSetting (Knob::Swell, -2.f);
  engine->recordPattern (take, aBeatFromNow, Measure{ 0, 1, 0 });
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  auto const params = take->getElevationParams ();
  EXPECT_FLOAT_EQ (params.elevationBase, 0.f);
  EXPECT_FLOAT_EQ (params.reach, 1.f);
  EXPECT_FLOAT_EQ (params.clipTop, 0.f);
  EXPECT_FLOAT_EQ (params.clipBottom, 0.f);
  EXPECT_FALSE (params.flat);
  EXPECT_EQ (take->getKnobStep (Knob::Sway), 0);
  EXPECT_EQ (take->getKnobStep (Knob::Swell), 0);
}

/** TOUCH over a clip with a band: the ticks nobody touched are moved into the
 *  take's whole-sphere band, so they play where the clip played them. */
TEST (TakeRecording, UntouchedTicksPlayWhereTheClipPlayedThem)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (240.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Touch);

  auto const old2D = Pos::fromCartesian (0.6f, 0.3f, 0.f);
  auto const clip = aClipStandingAt (old2D, 128);
  clip->setElevationBase (0.5f);
  clip->setReach (0.3f);
  auto const heardBefore = heightMap.mapTo3D (old2D, clip->getElevationParams ());

  // The take carries the clip's settings, as the UI hands it over.
  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->setElevationBase (0.5f);
  take->setReach (0.3f);
  engine->recordPattern (take, aBeatFromNow, Measure{ 0, 1, 0 }, clip);
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  auto const ticks = take->getTicks ().positions;
  ASSERT_FALSE (ticks.empty ());
  float worst = 0.f;
  for (std::size_t tick = 0; tick < ticks.size (); ++tick)
    {
      ASSERT_TRUE (ticks[tick].isValid ()) << "tick " << tick;
      worst = std::max (worst, angleBetween (heardOnFirstPass (heightMap, *take,
                                                               tick, 0.25f),
                                             heardBefore));
    }
  EXPECT_LT (worst, 2e-3f) << "an untouched tick moved";
}

/** The same over a clip whose band sways: each untouched tick is moved by the
 *  band the clip had at the phase that tick is heard with. */
TEST (TakeRecording, UntouchedTicksOfASwayingClipPlayWhereTheyWereHeard)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (240.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Touch);

  Measure const oneBar{ 1, 0, 0 };
  auto const old2D = Pos::fromCartesian (0.6f, 0.3f, 0.f);
  auto const clip = aClipStandingAt (old2D, 128);
  auto const swaying = [&] (Pattern &p) {
    p.setElevationBase (0.5f);
    p.setReach (0.3f);
    p.setKnobSetting (Knob::Sway, 4.f);
    p.setPlaybackLength (oneBar);
  };
  swaying (*clip);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  swaying (*take);
  engine->recordPattern (take, aBeatFromNow, oneBar, clip);
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  auto const ticksPerBar = static_cast<float> (TempoClock::getTicksPerBeat ())
                           * 4.f;
  auto const ticks = take->getTicks ().positions;
  ASSERT_FALSE (ticks.empty ());
  float worst = 0.f;
  float travelled = 0.f;
  Pos first = Pos::invalid;
  for (std::size_t tick = 0; tick < ticks.size (); ++tick)
    {
      ASSERT_TRUE (ticks[tick].isValid ()) << "tick " << tick;
      auto const at = ticksIntoFirstPass (static_cast<index_t> (tick),
                                          static_cast<index_t> (ticks.size ()),
                                          PlayDirection::Forward, ticksPerBar);
      setPassPhases (*clip, at, ticksPerBar);
      setPassPhases (*take, at, ticksPerBar);
      auto const before = playedPosition (heightMap, old2D, *clip);
      auto const now = playedPosition (heightMap, ticks[tick], *take);
      worst = std::max (worst, angleBetween (before, now));
      if (!first.isValid ())
        first = before;
      travelled = std::max (travelled, angleBetween (first, before));
    }
  EXPECT_GT (travelled, 0.05f) << "the sway moved nothing; the test means nothing";
  EXPECT_LT (worst, 2e-3f) << "an untouched tick of the swaying clip moved";
}

/** The clip is moved into the whole sphere when the take is asked for, on
 *  the caller's thread -- not at the downbeat, where the clock thread would
 *  stall on it. A 64-bar take over a clip whose knobs hold lanes read from a
 *  file (sparse: only where they change) is ready, band and all, before its
 *  downbeat, and quickly: that walk took 47 s on the clock (2026-10-08). */
TEST (TakeRecording, ALongTakeIsLaidOutBeforeItsDownbeat)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setTempoBPM (60.f);
  engine->setRecMode (RecMode::Touch);

  Measure const sixtyFourBars{ 64, 0, 0 };
  auto const ticks = static_cast<index_t> (64 * 4 * TempoClock::getTicksPerBeat ());
  auto const old2D = Pos::fromCartesian (0.6f, 0.3f, 0.f);
  auto const clip = aClipStandingAt (old2D, ticks);
  clip->setElevationBase (0.5f);
  clip->setReach (0.3f);
  KnobLanes lanes;
  for (auto const knob : { Knob::Rotate, Knob::SqueezeX, Knob::Tilt,
                           Knob::Reach, Knob::Elevation, Knob::Roll })
    lanes[static_cast<std::size_t> (knob)] = KnobLane::fromChangePoints (
        { { 5, 0.1f }, { static_cast<int> (ticks / 2), 0.2f } }, ticks);
  clip->setLanes (lanes);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->setElevationBase (0.5f);
  take->setReach (0.3f);
  take->setPlaybackLength (sixtyFourBars);

  auto const started = std::chrono::steady_clock::now ();
  engine->recordPattern (take, Measure{ 8, 0, 0 }, sixtyFourBars, clip);
  auto const took = std::chrono::duration<double> (
                        std::chrono::steady_clock::now () - started)
                        .count ();

  EXPECT_FALSE (engine->isRecording ()) << "the take started already";
  EXPECT_EQ (take->getNumTicks (), ticks);
  EXPECT_FLOAT_EQ (take->getElevationParams ().reach, 1.f)
      << "the band is opened at the downbeat, not when the take is asked for";
  EXPECT_FALSE (take->hasLane (Knob::Reach));
  EXPECT_TRUE (take->hasLane (Knob::Rotate)) << "the clip's other lanes went";
  EXPECT_LT (took, 1.0) << "laying out the take walked its lanes";
}

/** Before the downbeat the armed take owns the finger too, and the blob
 *  follows it wherever it goes: nothing is out of reach any more. */
TEST (TakeRecording, AnArmedTakeLetsTheBlobFollowTheFinger)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setTempoBPM (60.f);

  auto take = aTakeOverBuildPulse ();
  engine->recordPattern (take, Measure{ 8, 0, 0 }, Measure{ 1, 0, 0 });

  auto const finger = directionAt (0.1f, -1.2f);
  engine->setRecording3DPosition (finger);
  EXPECT_TRUE (waitUntil ([&] {
    return angleBetween (engine->getChannelPosition (0), finger) < 2e-3f;
  })) << "frac " << fracOf (engine->getChannelPosition (0));
  EXPECT_FALSE (engine->isRecording ())
      << "the take must not have started yet for this test to mean anything";
}

// ── The clip's rotate and squeeze are undone (#66) ───────────────────────

/** Decided 2026-10-07: a take inherits the clip's settings, so recording
 *  undoes the clip's rotate and squeeze -- and its spin, at the tick each
 *  point will be heard on. WRITE with the finger never down writes where the
 *  blob stood for the whole lap, which tests every tick of it. */
TEST (TakeRecording, ATakeOverATurnedClipIsHeardWhereTheBlobStood)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (240.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Write);

  auto take = aTakeOverATurnedSqueezedSpinningClip ();
  auto const standing = directionAt (0.3f, 2.f);
  engine->setChannel3DPosition (0, standing);

  engine->recordPattern (take, aBeatFromNow, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  auto const ticks = take->getTicks ().positions;
  ASSERT_FALSE (ticks.empty ());
  float worst = 0.f;
  std::size_t worstTick = 0;
  for (std::size_t tick = 0; tick < ticks.size (); ++tick)
    {
      ASSERT_TRUE (ticks[tick].isValid ()) << "tick " << tick;
      auto const miss = angleBetween (
          heardOnFirstPass (heightMap, *take, tick, 1.f), standing);
      if (miss > worst)
        {
          worst = miss;
          worstTick = tick;
        }
    }
  EXPECT_LT (worst, 2e-3f) << "tick " << worstTick << " of " << ticks.size ()
                           << " plays back turned by " << worst << " rad";
}

/** Played back, every lap of that take stands where the blob stood: the spin
 *  turns once a bar and the take is a bar long, so it is the same lap every
 *  time. Before, the take came back turned by the clip's rotate. */
TEST (TakeRecording, ATakeOverATurnedClipPlaysBackWhereItWasRecorded)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (240.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Write);

  auto take = aTakeOverATurnedSqueezedSpinningClip ();
  auto const standing = directionAt (0.3f, 2.f);
  engine->setChannel3DPosition (0, standing);

  engine->recordPattern (take, aBeatFromNow, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  // Somewhere else first, so a channel left standing would be caught.
  engine->setChannel3DPosition (0, directionAt (0.6f, -1.f));
  engine->setRecordingMode (MotionEngine::RecordingMode::Loop);
  engine->playPattern (take, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return take->getStatus () == Pattern::Status::Playing; }));
  juce::Thread::sleep (50);

  float worst = 0.f;
  for (int sample = 0; sample < 30; ++sample)
    {
      worst = std::max (worst,
                        angleBetween (engine->getChannelPosition (0), standing));
      juce::Thread::sleep (20);
    }
  EXPECT_LT (worst, 1e-2f) << "the take played back turned or stretched";
}

/** The blob during the take is what will play back, and with the finger
 *  inside the band that is the finger: "Build Pulse" turned, squeezed, leant
 *  and spinning, TOUCH, the finger down. */
TEST (TakeRecording, TheBlobOverATurnedClipIsWhereTheTakeWillPlay)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setTempoBPM (120.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::Loop);
  engine->setRecMode (RecMode::Touch);

  auto take = aTakeOverATurnedSqueezedSpinningClip ();
  take->setElevationBase (0.35f);
  take->setReach (0.7f);
  engine->recordPattern (take, Measure{}, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine->isRecording (); }));

  auto const finger = turnedInSpace (directionAt (0.55f, -0.7f),
                                     SpaceTurn{ 0.25f, 0.f });
  engine->setRecording3DPosition (finger);
  ASSERT_TRUE (waitUntil ([&] { return anyTickWritten (*take); }));
  juce::Thread::sleep (20);

  EXPECT_LT (angleBetween (engine->getChannelPosition (0), finger), 2e-3f)
      << "the blob left the finger inside the band";

  auto const ticks = take->getTicks ().positions;
  float worst = 0.f;
  for (std::size_t tick = 0; tick < ticks.size (); ++tick)
    if (ticks[tick].isValid ())
      worst = std::max (worst,
                        angleBetween (heardOnFirstPass (heightMap, *take, tick,
                                                        1.f),
                                      finger));
  EXPECT_LT (worst, 2e-3f) << "a written tick plays back away from the finger";
}

// ── What the rec modes do to the clip underneath ─────────────────────────

/** WRITE replaces the clip: one pass, untouched, leaves nothing of the old
 *  path -- every tick is where the blob stood as the take began. */
TEST (TakeRecording, WriteClearsTheOldPathForTheWholeLap)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (240.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Write);

  auto const old2D = Pos::fromCartesian (0.9f, 0.f, 0.f);
  auto const clip = aClipStandingAt (old2D, 128);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  auto const standing = directionAt (0.3f, 2.f);
  engine->setChannel3DPosition (0, standing);
  auto const oldHeard = heightMap.mapTo3D (old2D, clip->getElevationParams ());

  engine->recordPattern (take, aBeatFromNow, Measure{ 0, 1, 0 }, clip);
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  auto const ticks = take->getTicks ().positions;
  ASSERT_FALSE (ticks.empty ());
  auto const oldLeft = std::count_if (
      ticks.begin (), ticks.end (),
      [&] (Pos const &p) { return heardAt (heightMap, *take, p, oldHeard); });
  auto const standingWritten = std::count_if (
      ticks.begin (), ticks.end (),
      [&] (Pos const &p) { return heardAt (heightMap, *take, p, standing); });

  EXPECT_EQ (oldLeft, 0) << "of " << ticks.size () << " ticks";
  EXPECT_EQ (standingWritten, static_cast<long> (ticks.size ()));
}

/** TOUCH is an overdub: a pass nobody touched leaves the clip's path as it
 *  was, and a touch changes only the ticks it was down for. */
TEST (TakeRecording, TouchKeepsTheTicksItDidNotTouch)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (120.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Touch);

  auto const old2D = Pos::fromCartesian (0.9f, 0.f, 0.f);
  auto const clip = aClipStandingAt (old2D, 128);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  auto const finger = directionAt (0.3f, 2.f);
  auto const oldHeard = heightMap.mapTo3D (old2D, clip->getElevationParams ());

  // One beat at 120: half a second, the finger down for roughly its middle.
  engine->recordPattern (take, aBeatFromNow, Measure{ 0, 1, 0 }, clip);
  ASSERT_TRUE (waitUntil ([&] { return engine->isRecording (); }));
  ASSERT_TRUE (waitUntil (
      [&] { return engine->getRecordingProgress () > 0.25f; }));
  engine->setRecording3DPosition (finger);
  ASSERT_TRUE (waitUntil (
      [&] { return engine->getRecordingProgress () > 0.5f; }));
  engine->releaseRecordingPosition ();
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  auto const ticks = take->getTicks ().positions;
  auto const oldLeft = std::count_if (
      ticks.begin (), ticks.end (),
      [&] (Pos const &p) { return heardAt (heightMap, *take, p, oldHeard); });
  auto const touched = std::count_if (
      ticks.begin (), ticks.end (),
      [&] (Pos const &p) { return heardAt (heightMap, *take, p, finger); });

  EXPECT_GT (oldLeft, 0) << "the overdub wiped the clip";
  EXPECT_GT (touched, 0) << "the touch wrote nothing";
  EXPECT_EQ (oldLeft + touched, static_cast<long> (ticks.size ()))
      << "a tick is neither the clip's nor the finger's";
  // The ticks before the finger came down are the clip's.
  EXPECT_TRUE (heardAt (heightMap, *take, ticks.front (), oldHeard));
}

// ── 3D, FREQ and Q belong to the actions ─────────────────────────────────

/** Decided 2026-10-07: a clip and its takes carry every setting except the
 *  three audio ones. Turning them during a take reaches Core as always, and
 *  writes nothing into the take -- no lane, nothing in the file -- so the take
 *  never plays them back. */
TEST (TakeRecording, ATakeRecordsNo3dFreqOrQ)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (240.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  // Touch, because Write writes every Motion and Elevation knob through the
  // whole pass by design -- a lane there would not be the pots'.
  engine->setRecMode (RecMode::Touch);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  engine->recordPattern (take, aBeatFromNow, Measure{ 0, 1, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine->isRecording (); }));
  engine->setRecording3DPosition (Pos::fromCartesian (0.f, 0.f, 1.f));

  for (int step = 0; step < 10; ++step)
    {
      auto const value = 0.1f + 0.08f * static_cast<float> (step);
      engine->setChannelPot1 (0, value);
      engine->setChannelPot2 (0, 1.f - value);
      engine->setChannelPot3 (0, value);
      juce::Thread::sleep (15);
    }
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  EXPECT_FALSE (take->hasLanes ()) << "the pots wrote a lane";

  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-take-no-pots.json");
  Clip clip;
  clip.name = "Take";
  clip.lanes = take->getLanes ();
  ASSERT_TRUE (ClipFile::save (clip, file));
  auto const parsed = juce::JSON::parse (file.loadFileAsString ());
  file.deleteFile ();
  EXPECT_FALSE (parsed.hasProperty ("lanes"));

  // Played back, the take leaves the pots where the hand last put them.
  engine->setChannelPot1 (0, 0.2f);
  engine->setChannelPot2 (0, 0.3f);
  engine->setChannelPot3 (0, 0.4f);
  engine->setRecordingMode (MotionEngine::RecordingMode::Loop);
  engine->playPattern (take, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return take->getStatus () == Pattern::Status::Playing; }));
  juce::Thread::sleep (300);
  EXPECT_FLOAT_EQ (engine->getChannelPot1 (0), 0.2f);
  EXPECT_FLOAT_EQ (engine->getChannelPot2 (0), 0.3f);
  EXPECT_FLOAT_EQ (engine->getChannelPot3 (0), 0.4f);
}

/** A clip file with pot lanes in it -- none was ever written, the lanes have
 *  only ever known the Motion and Elevation knobs, but a hand-edited or
 *  foreign file could carry them -- still loads, plays its knob lanes, and
 *  drops the pots: saving it again leaves them out. */
TEST (TakeRecording, PotLanesInAClipFileAreIgnored)
{
  auto const file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("a3-clip-with-pot-lanes.json");
  file.replaceWithText (R"({
    "name": "Old",
    "lanes": {
      "ticks": 16,
      "rot": [[0, 0.25], [8, 0.5]],
      "3d": [[0, 0.9]],
      "freq": [[0, 0.1], [4, 0.8]],
      "q": [[0, 0.7]]
    }
  })");

  auto const loaded = ClipFile::load (file);
  ASSERT_TRUE (loaded.has_value ());
  auto const &rotate = loaded->lanes[static_cast<std::size_t> (Knob::Rotate)];
  EXPECT_FLOAT_EQ (rotate.at (9.0).value_or (-1.f), 0.5f)
      << "the knob lane beside the pots was lost";

  ASSERT_TRUE (ClipFile::save (*loaded, file));
  auto const lanes
      = juce::JSON::parse (file.loadFileAsString ()).getProperty ("lanes", {});
  file.deleteFile ();
  EXPECT_TRUE (lanes.hasProperty ("rot"));
  EXPECT_FALSE (lanes.hasProperty ("3d"));
  EXPECT_FALSE (lanes.hasProperty ("freq"));
  EXPECT_FALSE (lanes.hasProperty ("q"));
}

// ── Measurement: how densely a fast drag is written ──────────────────────

/** Not a rule, a number to watch (#66 point 3): a finger sampled at 120 Hz,
 *  dragged fast round a ring, against an engine writing once per tick (128
 *  a beat, 256 a second at 120 BPM). Prints how many distinct positions the
 *  take holds per second and the largest step between neighbouring ticks.
 *  The assertion is only that no finger sample is lost wholesale. */
TEST (TakeRecording, MeasureHowAFastDragIsSampled)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setPreviewMode (0, true);
  engine->setTempoBPM (120.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  engine->setRecMode (RecMode::Touch);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  auto const params = take->getElevationParams ();

  // One bar at 120: two seconds, 512 ticks.
  engine->recordPattern (take, aBeatFromNow, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine->isRecording (); }));

  constexpr double fingerHz = 120.0;
  constexpr double turnsPerSecond = 2.0; // a fast circle
  auto const startMs = juce::Time::getMillisecondCounterHiRes ();
  int samplesSent = 0;
  while (engine->isRecording ())
    {
      auto const t = (juce::Time::getMillisecondCounterHiRes () - startMs)
                     / 1000.0;
      auto const azimuth = static_cast<float> (
          2.0 * juce::MathConstants<double>::pi * turnsPerSecond * t);
      engine->setRecording3DPosition (directionAt (0.3f, azimuth));
      ++samplesSent;
      // Paced to the next 1/120 s from the start rather than slept a fixed
      // step, so a late wake does not slow the drag down.
      auto const nextMs = startMs + 1000.0 * samplesSent / fingerHz;
      auto const waitMs = nextMs - juce::Time::getMillisecondCounterHiRes ();
      if (waitMs > 0)
        juce::Thread::sleep (static_cast<int> (waitMs));
    }
  engine->releaseRecordingPosition ();
  auto const seconds
      = (juce::Time::getMillisecondCounterHiRes () - startMs) / 1000.0;

  auto const ticks = take->getTicks ().positions;
  int distinct = 0;
  float largestStepDegrees = 0.f;
  for (std::size_t i = 1; i < ticks.size (); ++i)
    {
      if (!ticks[i].isValid () || !ticks[i - 1].isValid ())
        continue;
      if (!near2D (ticks[i], ticks[i - 1]))
        ++distinct;
      largestStepDegrees = std::max (
          largestStepDegrees,
          juce::radiansToDegrees (
              angleBetween (heightMap.mapTo3D (ticks[i], params),
                            heightMap.mapTo3D (ticks[i - 1], params))));
    }
  auto const ringDegreesPerSample
      = static_cast<float> (360.0 * turnsPerSecond / fingerHz);

  std::cout << "[ measure  ] finger " << samplesSent << " samples in "
            << seconds << " s (" << samplesSent / seconds << " Hz); take "
            << ticks.size () << " ticks, " << distinct
            << " distinct positions (" << distinct / seconds
            << " per s); largest step " << largestStepDegrees
            << " deg on the sphere (azimuth per finger sample "
            << ringDegreesPerSample << " deg on a ring at frac 0.3)"
            << std::endl;
  RecordProperty ("fingerSamples", samplesSent);
  RecordProperty ("distinctPositions", distinct);
  RecordProperty ("largestStepMilliDegrees",
                  static_cast<int> (largestStepDegrees * 1000.f));

  EXPECT_GT (distinct, samplesSent / 2)
      << "most finger samples never reached the take";
}
