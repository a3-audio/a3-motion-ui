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
walk (Measure length, PlayDirection direction = PlayDirection::Forward)
{
  auto pattern = std::make_shared<Pattern> ();
  auto const ticks = static_cast<index_t> (Measure::convertToTicks (length, 4));
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
  pattern->setPlaybackLength (length);
  pattern->setPlayDirection (direction);
  return pattern;
}

std::shared_ptr<Pattern>
fourBarWalk ()
{
  return walk (Measure{ 4, 0, 0 });
}

int
ticksIn (Measure length)
{
  return Measure::convertToTicks (length, 4);
}

/** Where a forward clip's place and its time through the lap agree: the
 *  position is the lap's share, so neither went back without the other. */
void
expectPlaceMatchesLap (Pattern const &clip, Measure length)
{
  auto const lap = static_cast<float> (clip.getLapTick ())
                   / static_cast<float> (ticksIn (length));
  EXPECT_NEAR (clip.getPlayPosition (), lap, 1e-3f)
      << "the place and the lap disagree";
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

/** Starts `clip` on the next downbeat, and schedules a pause `bars` later. */
Measure
playThenPauseAfter (Engine &e, std::shared_ptr<Pattern> const &clip, int bars)
{
  EXPECT_TRUE (waitUntil ([&] { return e.current () != Measure{}; }));
  auto const start = TempoClock::nextDownBeat (e.current ());
  e.engine.playPattern (clip, start);
  e.engine.pausePattern (clip, Measure{ start.bar () + bars, 0, 0 });
  EXPECT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Playing; }));
  EXPECT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Idle; }, 8000));
  return start;
}

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

// The reviewer's case: a pass that is not whole bars (six beats). Paused on
// the downbeat two bars in, it is a third of the way through -- the old snap
// to the clip's bars put it back at the top.
TEST (PauseResumes, ADownbeatPauseInAnOddLengthClipStaysPut)
{
  Engine e;
  Measure const sixBeats{ 1, 2, 0 };
  auto clip = walk (sixBeats);
  playThenPauseAfter (e, clip, 2);

  EXPECT_EQ (static_cast<int> (clip->getLapTick ()),
             2 * TempoClock::getTicksPerBeat ())
      << "two bars in is two beats into the second pass";
  expectPlaceMatchesLap (*clip, sixBeats);
  EXPECT_TRUE (clip->resumesOnPlay ());
}

// Shift: at once, then back to the start of the bar of the music it was in --
// place and lap together, so the progress bar does not run a bar ahead.
TEST (PauseResumes, AShiftPauseGoesBackToItsBarPlaceAndLapAlike)
{
  Engine e;
  auto clip = fourBarWalk ();
  ASSERT_TRUE (waitUntil ([&] { return e.current () != Measure{}; }));
  e.engine.playPattern (clip, TempoClock::nextDownBeat (e.current ()));
  ASSERT_TRUE (waitUntil ([&] { return clip->getPlayPosition () > 0.3f; }));
  e.engine.pausePattern (clip, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Idle; }));

  auto const ticksPerBar = 4 * TempoClock::getTicksPerBeat ();
  EXPECT_EQ (static_cast<int> (clip->getLapTick ()) % ticksPerBar, 0)
      << "the lap went back to its bar's start";
  EXPECT_GE (static_cast<int> (clip->getLapTick ()), ticksPerBar);
  expectPlaceMatchesLap (*clip, Measure{ 4, 0, 0 });
}

// Play|Pause is now since 2026-10-08, and the resume is now too: the place
// must stay exactly where the hand stopped it, not a bar's start before.
TEST (PauseResumes, AnExactPauseKeepsThePlaceOffTheBar)
{
  Engine e;
  auto clip = fourBarWalk ();
  ASSERT_TRUE (waitUntil ([&] { return e.current () != Measure{}; }));
  e.engine.playPattern (clip, TempoClock::nextDownBeat (e.current ()));
  ASSERT_TRUE (waitUntil ([&] { return clip->getPlayPosition () > 0.3f; }));
  e.engine.pausePattern (clip, Measure{}, PausePlace::Exact);
  ASSERT_TRUE (waitUntil (
      [&] { return clip->getStatus () == Pattern::Status::Idle; }));

  auto const ticksPerBar = 4 * TempoClock::getTicksPerBeat ();
  EXPECT_NE (static_cast<int> (clip->getLapTick ()) % ticksPerBar, 0)
      << "the lap stayed where it was, mid-bar";
  EXPECT_TRUE (clip->resumesOnPlay ());
  expectPlaceMatchesLap (*clip, Measure{ 4, 0, 0 });
}

// A one-bar bounce is home every second bar, arriving with its sign still
// turned. Paused there, it has to set out again, not come back from the far
// end.
TEST (PauseResumes, ABouncePausedAtHomeSetsOutAgain)
{
  Engine e;
  auto clip = walk (Measure{ 1, 0, 0 }, PlayDirection::Bounce);
  playThenPauseAfter (e, clip, 2);
  EXPECT_LT (clip->getPlayPosition (), 0.05f) << "two bars in is home";

  e.engine.playPattern (clip, Measure{});
  ASSERT_TRUE (waitUntil ([&] { return clip->getPlayPosition () > 0.05f; }));
  EXPECT_GT (clip->getPlaySign (), 0.f) << "it came back from the far end";
  EXPECT_LT (clip->getPlayPosition (), 0.5f);
}
