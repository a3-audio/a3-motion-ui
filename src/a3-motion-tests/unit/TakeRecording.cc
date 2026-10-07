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
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <algorithm>
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

/** The lowest colatitude a clip can play, by running the forward map over
 *  the pad -- see playableRange in HeightMapSphere.cc. */
float
bandTopOf (HeightMapSphere const &heightMap, ElevationParams const &params)
{
  auto lowest = 1.f;
  for (int i = 0; i <= 80000; ++i)
    lowest = std::min (
        lowest,
        fracOf (heightMap.mapTo3D (
            Pos::fromCartesian (4.f * static_cast<float> (i) / 80000.f, 0.f,
                                0.f),
            params)));
  return lowest;
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

bool
near2D (Pos const &a, Pos const &b)
{
  return a.isValid () && b.isValid ()
         && std::hypot (a.x () - b.x (), a.y () - b.y ()) < 1e-3f;
}

}

// ── The take keeps the clip's band (#66) ─────────────────────────────────

/** What the blob does while the take runs is what the take plays back: the
 *  finger above the band is held on its edge, live as well as written. It
 *  used to follow the finger exactly, which sounded right while drawing and
 *  saved a ring. */
TEST (TakeRecording, ATakeIsHeardWhereItIsStored)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setTempoBPM (120.f);
  engine->setRecordingMode (MotionEngine::RecordingMode::Loop);
  engine->setRecMode (RecMode::Touch);

  auto take = aTakeOverBuildPulse ();
  auto const params = take->getElevationParams ();
  engine->recordPattern (take, Measure{}, Measure{ 1, 0, 0 });
  ASSERT_TRUE (waitUntil ([&] { return engine->isRecording (); }));

  constexpr float azimuth = 0.4f;
  engine->setRecording3DPosition (directionAt (0.1f, azimuth));
  ASSERT_TRUE (waitUntil ([&] { return anyTickWritten (*take); }));
  // The position and the tick are written on the same engine tick; one more
  // turn of the clock settles any read that raced it.
  juce::Thread::sleep (20);

  auto const live = engine->getChannelPosition (0);
  auto const ticks = take->getTicks ().positions;
  auto const written = std::find_if (ticks.begin (), ticks.end (),
                                     [] (Pos const &p) { return p.isValid (); });
  ASSERT_NE (written, ticks.end ());
  auto const played = heightMap.mapTo3D (*written, params);

  EXPECT_NEAR (fracOf (live), bandTopOf (heightMap, params), 2e-3f)
      << "the blob left the band while recording";
  EXPECT_NEAR (std::atan2 (live.y (), live.x ()), azimuth, 2e-3f);
  EXPECT_LT (angleBetween (live, played), 2e-3f)
      << "what was heard while recording is not what plays back";
}

/** The same before the downbeat: an armed take already owns the finger, and
 *  a blob that left the band there would jump into it as the take began. */
TEST (TakeRecording, AnArmedTakeHoldsTheBlobInTheBandToo)
{
  HeightMapSphere heightMap;
  auto engine = anOfflineEngine (heightMap);
  engine->setTempoBPM (60.f);

  auto take = aTakeOverBuildPulse ();
  auto const params = take->getElevationParams ();
  // Far enough out that the take is certainly still waiting.
  engine->recordPattern (take, Measure{ 8, 0, 0 }, Measure{ 1, 0, 0 });

  constexpr float azimuth = -1.2f;
  engine->setRecording3DPosition (directionAt (0.1f, azimuth));
  auto const edge = bandTopOf (heightMap, params);
  EXPECT_TRUE (waitUntil ([&] {
    return std::abs (fracOf (engine->getChannelPosition (0)) - edge) < 2e-3f;
  })) << "frac " << fracOf (engine->getChannelPosition (0)) << ", band edge "
      << edge;
  EXPECT_FALSE (engine->isRecording ())
      << "the take must not have started yet for this test to mean anything";
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
  auto const standing2D
      = heightMap.mapTo2D (standing, take->getElevationParams ());

  engine->recordPattern (take, aBeatFromNow, Measure{ 0, 1, 0 }, clip);
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";

  auto const ticks = take->getTicks ().positions;
  ASSERT_FALSE (ticks.empty ());
  auto const oldLeft = std::count_if (
      ticks.begin (), ticks.end (),
      [&] (Pos const &p) { return near2D (p, old2D); });
  auto const standingWritten = std::count_if (
      ticks.begin (), ticks.end (),
      [&] (Pos const &p) { return near2D (p, standing2D); });

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
  auto const finger2D = heightMap.mapTo2D (finger, take->getElevationParams ());

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
      [&] (Pos const &p) { return near2D (p, old2D); });
  auto const touched = std::count_if (
      ticks.begin (), ticks.end (),
      [&] (Pos const &p) { return near2D (p, finger2D); });

  EXPECT_GT (oldLeft, 0) << "the overdub wiped the clip";
  EXPECT_GT (touched, 0) << "the touch wrote nothing";
  EXPECT_EQ (oldLeft + touched, static_cast<long> (ticks.size ()))
      << "a tick is neither the clip's nor the finger's";
  // The ticks before the finger came down are the clip's.
  EXPECT_TRUE (near2D (ticks.front (), old2D));
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
