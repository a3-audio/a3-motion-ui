/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <cmath>
#include <mutex>

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

using namespace a3;

// ❚❚ resumes where it stopped, ■ goes back to the top (maintainer,
// 2026-10-08). These run the real tempo clock: pause and resume happen on
// ticks.

namespace
{
/** A four-bar clip walking once round the floor. */
std::shared_ptr<Pattern>
fourBarWalk ()
{
  auto pattern = std::make_shared<Pattern> ();
  auto const ticks = static_cast<index_t> (TempoClock::getTicksPerBeat () * 16);
  pattern->resize (ticks);
  for (index_t tick = 0; tick < ticks; ++tick)
    {
      auto const angle = 6.2831853f * static_cast<float> (tick)
                         / static_cast<float> (ticks);
      pattern->setTick (tick, Pos::fromCartesian (0.6f * std::cos (angle),
                                                  0.6f * std::sin (angle),
                                                  0.f));
    }
  pattern->markComplete ();
  pattern->setChannel (0);
  pattern->setPlaybackLength (Measure{ 4, 0, 0 });
  return pattern;
}

struct Engine
{
  HeightMapSphere heightMap;
  MotionEngine engine{ 4, heightMap, offlineBackend () };
  std::mutex mutex;
  Measure now;
  TempoClock::PointerT handle;

  Engine ()
  {
    engine.setPreviewMode (0, true); // nothing leaves the machine from a test
    engine.setTempoBPM (240.f);
    handle = engine.getTempoClock ().scheduleEventHandlerAddition (
        [this] (Measure measure) {
          std::lock_guard<std::mutex> lock (mutex);
          now = measure;
        },
        TempoClock::Event::Tick, TempoClock::Execution::TimerThread);
  }

  Measure
  current ()
  {
    std::lock_guard<std::mutex> lock (mutex);
    return now;
  }
};

/** Plays `clip` from a downbeat past its first bar, then pauses it on a
 *  downbeat. Answers where it stands. */
float
playThenPause (Engine &e, std::shared_ptr<Pattern> const &clip)
{
  auto &engine = e.engine;
  // Started on a downbeat, as Play|Pause starts it.
  EXPECT_TRUE (waitUntil ([&] { return e.current () != Measure{}; }));
  engine.playPattern (clip, TempoClock::nextDownBeat (e.current ()));
  EXPECT_TRUE (waitUntil ([&] { return clip->getPlayPosition () > 0.3f; }));
  engine.pausePattern (clip,
                       TempoClock::nextDownBeat (e.current ()));
  EXPECT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Idle; }));
  return clip->getPlayPosition ();
}
}

TEST (PauseResumes, APauseKeepsThePlaceOnTheBar)
{
  Engine e;
  auto clip = fourBarWalk ();
  auto const paused = playThenPause (e, clip);

  EXPECT_GT (paused, 0.3f);
  EXPECT_NEAR (std::fmod (paused, 0.25f), 0.f, 1e-4f)
      << "a pause on the downbeat stands on a bar of the clip";
  EXPECT_TRUE (clip->resumesOnPlay ());
}

TEST (PauseResumes, PlayAfterAPauseGoesOnFromThere)
{
  Engine e;
  auto clip = fourBarWalk ();
  auto const paused = playThenPause (e, clip);

  e.engine.playPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Playing; }));
  auto const resumed = clip->getPlayPosition ();
  EXPECT_GE (resumed, paused - 1e-4f) << "it went back to the top";
  EXPECT_LT (resumed, paused + 0.25f);
  EXPECT_FALSE (clip->resumesOnPlay ()) << "a resume is used once";
}

TEST (PauseResumes, StopForgetsThePause)
{
  Engine e;
  auto clip = fourBarWalk ();
  playThenPause (e, clip);

  e.engine.stopPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil ([&] { return !clip->resumesOnPlay (); }));

  e.engine.playPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Playing; }));
  EXPECT_LT (clip->getPlayPosition (), 0.1f) << "■ goes back to the top";
}

TEST (PauseResumes, AStopWhilePlayingStillStartsFromTheTop)
{
  Engine e;
  auto clip = fourBarWalk ();
  e.engine.playPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil ([&] { return clip->getPlayPosition () > 0.3f; }));
  e.engine.stopPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Idle; }));
  EXPECT_FALSE (clip->resumesOnPlay ());

  e.engine.playPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Playing; }));
  EXPECT_LT (clip->getPlayPosition (), 0.1f);
}
