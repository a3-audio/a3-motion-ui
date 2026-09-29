/*

  A3 Motion UI
  Copyright (C) 2026 A3 Audio

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

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <mutex>
#include <vector>

using namespace a3;

// The end action Clip: when a clip's pass runs out, the clip it names takes
// the channel over. These run the real tempo clock, because the hand-over is
// decided on the tick the pass ends -- nothing a pure test can reach.

namespace
{
/** A clip standing on one spot for its whole pass, one beat long, so the
 *  channel's position says which of two clips is playing. */
std::shared_ptr<Pattern>
clipStandingAt (float x, EndAction end)
{
  auto pattern = std::make_shared<Pattern> ();
  auto const ticks = static_cast<index_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (ticks);
  for (index_t tick = 0; tick < ticks; ++tick)
    pattern->setTick (tick, Pos::fromCartesian (x, 0.f, 0.f));
  pattern->markComplete ();
  pattern->setChannel (0);
  pattern->setEndAction (end);
  pattern->setPlaybackLength (Measure{ 0, 1, 0 });
  return pattern;
}

/** What the channel looked like after the engine's tick: the handler is
 *  added after the engine's own, and a clock calls its handlers in order. */
struct Sample
{
  long long tick;
  Pattern::Status first;
  Pattern::Status follow;
  index_t followLap;
  Pos position;
};

class TickRecorder
{
public:
  TickRecorder (MotionEngine &engine, std::shared_ptr<Pattern> first,
                std::shared_ptr<Pattern> follow)
  {
    _samples.reserve (8192);
    _handle = engine.getTempoClock ().scheduleEventHandlerAddition (
        [this, &engine, first, follow] (Measure measure) {
          std::lock_guard<std::mutex> lock (_mutex);
          if (_samples.size () == _samples.capacity ())
            return;
          _samples.push_back (
              { Measure::convertToTicks (measure, engine.getBeatsPerBar ()),
                first->getStatus (), follow->getStatus (),
                follow->getLapTick (), engine.getChannelPosition (0) });
        },
        TempoClock::Event::Tick, TempoClock::Execution::TimerThread);
  }

  std::vector<Sample>
  samples ()
  {
    std::lock_guard<std::mutex> lock (_mutex);
    return _samples;
  }

private:
  std::mutex _mutex;
  std::vector<Sample> _samples;
  TempoClock::PointerT _handle;
};

int
firstIndexWhere (std::vector<Sample> const &samples, bool (*test) (Sample const &))
{
  for (size_t i = 0; i < samples.size (); ++i)
    if (test (samples[i]))
      return static_cast<int> (i);
  return -1;
}
}

// The follow starts on the tick the pass ends -- the tick a looping clip
// would have gone back to its top on -- rather than on the next downbeat
// after it. No tick without a clip, no tick with both, and the position the
// channel is given on that tick is already the follow's.
TEST (EndActionClip, TheFollowTakesOverOnTheTickThePassEnds)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true); // nothing leaves the machine from a test
  engine.setTempoBPM (240.f);

  auto first = clipStandingAt (0.6f, EndAction::Clip);
  auto follow = clipStandingAt (-0.6f, EndAction::Loop);

  TickRecorder recorder (engine, first, follow);
  engine.armFollowPattern (0, first, follow);
  engine.playPattern (first, Measure{});

  ASSERT_TRUE (waitUntil ([&] {
    auto const samples = recorder.samples ();
    auto const taken = firstIndexWhere (samples, [] (Sample const &s) {
      return s.follow == Pattern::Status::Playing;
    });
    return taken >= 0 && static_cast<size_t> (taken) + 4 < samples.size ();
  })) << "the follow never started";

  auto const samples = recorder.samples ();
  auto const started = firstIndexWhere (samples, [] (Sample const &s) {
    return s.first == Pattern::Status::Playing;
  });
  auto const taken = firstIndexWhere (samples, [] (Sample const &s) {
    return s.follow == Pattern::Status::Playing;
  });
  ASSERT_GE (started, 0);
  ASSERT_GT (taken, started);

  auto const &before = samples[static_cast<size_t> (taken - 1)];
  auto const &at = samples[static_cast<size_t> (taken)];
  auto const &after = samples[static_cast<size_t> (taken + 3)];

  EXPECT_EQ (before.first, Pattern::Status::Playing)
      << "a tick went by with nothing playing: the follow waited";
  EXPECT_EQ (at.first, Pattern::Status::Idle)
      << "both clips are playing at once";
  EXPECT_EQ (at.tick, before.tick + 1) << "ticks were missed while sampling";

  // One pass of the first clip is one beat. It started on the tick sampled
  // first, reached its end a beat of ticks later -- the tick a loop wraps on,
  // where a looping clip's lap is back at 0 -- and the follow is there.
  auto const passTicks = TempoClock::getTicksPerBeat ();
  EXPECT_EQ (at.tick - samples[static_cast<size_t> (started)].tick,
             passTicks - 1);
  EXPECT_EQ (at.followLap, 0u) << "the follow is not at the top of its pass";

  EXPECT_EQ (at.position, after.position)
      << "the tick of the hand-over still wrote the first clip's position";
  EXPECT_NE (at.position, before.position);
}

// Nothing armed: the pass ends as a stop ends it, back at the take's start
// and out of playback. A missing or unknown follow clip is armed as nothing,
// so this is what those do too.
TEST (EndActionClip, WithoutAFollowTheClipStopsAsStopDoes)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto first = clipStandingAt (0.6f, EndAction::Clip);
  engine.playPattern (first, Measure{});

  ASSERT_TRUE (waitUntil (
      [&] { return first->getStatus () == Pattern::Status::Playing; }));
  EXPECT_TRUE (waitUntil (
      [&] { return first->getStatus () == Pattern::Status::Idle; }))
      << "it kept going";
  EXPECT_FLOAT_EQ (first->getPlayPosition (), 0.f);
  EXPECT_EQ (engine.getPlayingPattern (0), nullptr);
}

// A follow belongs to the clip it was armed for. The channel may hold another
// clip by the time a pass ends -- chosen on FILES, say -- and that one's end
// must not hand over to a clip picked for somebody else.
TEST (EndActionClip, AFollowArmedForAnotherClipIsNotTaken)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto first = clipStandingAt (0.6f, EndAction::Clip);
  auto other = clipStandingAt (0.2f, EndAction::Clip);
  auto follow = clipStandingAt (-0.6f, EndAction::Loop);
  engine.armFollowPattern (0, other, follow);
  engine.playPattern (first, Measure{});

  ASSERT_TRUE (waitUntil (
      [&] { return first->getStatus () == Pattern::Status::Playing; }));
  ASSERT_TRUE (waitUntil (
      [&] { return first->getStatus () == Pattern::Status::Idle; }));
  juce::Thread::sleep (50);
  EXPECT_NE (follow->getStatus (), Pattern::Status::Playing);
}

// Play pressed on a running clip means "finish this pass and stop", whatever
// the end action says. A chain is one of the things it gets you out of.
TEST (EndActionClip, AskingItToFinishStopsInsteadOfFollowing)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto first = clipStandingAt (0.6f, EndAction::Clip);
  auto follow = clipStandingAt (-0.6f, EndAction::Loop);
  engine.armFollowPattern (0, first, follow);
  engine.playPattern (first, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return first->getStatus () == Pattern::Status::Playing; }));
  engine.stopPatternAtEnd (first);

  ASSERT_TRUE (waitUntil (
      [&] { return first->getStatus () == Pattern::Status::Idle; }));
  juce::Thread::sleep (50);
  EXPECT_NE (follow->getStatus (), Pattern::Status::Playing);
}

// End actions are playback's. A take that runs out is a recording ending,
// and it must not throw the channel to another clip -- the take waits in its
// slot for SAVE or DISCARD.
TEST (EndActionClip, ATakeEndingDoesNotFollow)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);
  engine.setRecordingMode (MotionEngine::RecordingMode::OneShot);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  take->setEndAction (EndAction::Clip);
  auto follow = clipStandingAt (-0.6f, EndAction::Loop);
  engine.armFollowPattern (0, take, follow);

  engine.recordPattern (take, Measure{}, Measure{ 0, 1, 0 });
  // Asked as "has been recording and is over": a take whose timepoint has
  // already passed can run for a single tick, too short to be caught at it.
  ASSERT_TRUE (waitUntil ([&] {
    return take->wasRecording ()
           && take->getStatus () == Pattern::Status::Idle;
  })) << "the take never ended";
  juce::Thread::sleep (50);
  EXPECT_NE (follow->getStatus (), Pattern::Status::Playing);
}

// The accent running out applies the end action -- Stop and Pause end the
// pass there. Clip does not: its follow is bound to the bar grid the pass
// ends on, and an accent ends wherever a finger let go. It keeps running
// and hands over at the end of its pass, the way Loop keeps running.
TEST (EndActionClip, AnAccentRunningOutLeavesTheChainToThePassEnd)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto first = clipStandingAt (0.6f, EndAction::Clip);
  first->setPlaybackLength (Measure{ 4, 0, 0 });
  first->setEnvelopeAttack (0);
  first->setEnvelopeDecay (0);
  engine.playPattern (first, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return first->getStatus () == Pattern::Status::Playing; }));

  engine.setChannelAction (0, std::nullopt);
  engine.setChannelAccentHeld (0, true, first);
  ASSERT_TRUE (waitUntil ([&] { return engine.isChannelAccentActive (0); }));
  engine.setChannelAccentHeld (0, false, first);
  ASSERT_TRUE (waitUntil ([&] { return !engine.isChannelAccentActive (0); }));

  EXPECT_EQ (first->getStatus (), Pattern::Status::Playing)
      << "the accent ended the pass off the grid";
}
