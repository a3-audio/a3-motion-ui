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

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/flight/FlightField.hh>

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <vector>

using namespace a3;

// The engine flies an ORBIT channel on the clock thread, through the same
// channel position playback writes, so the send loop is the one a clip goes
// out through. These run the real tempo clock: the switches happen on
// whichever tick the message thread's store lands on, and the pacer counts
// wall-clock milliseconds -- nothing a pure test reaches.

namespace
{
constexpr float clipX = 0.5f;
constexpr float bpm = 240.f;
constexpr double jumpDegrees = 5.;

/** A clip standing on one spot, a beat long and looping: whatever moves the
 *  channel away from that spot is the flight, not the clip. */
std::shared_ptr<Pattern>
clipStandingOn (index_t channel)
{
  auto pattern = std::make_shared<Pattern> ();
  auto const ticks = static_cast<index_t> (TempoClock::getTicksPerBeat ());
  pattern->resize (ticks);
  for (index_t tick = 0; tick < ticks; ++tick)
    pattern->setTick (tick, Pos::fromCartesian (clipX, 0.f, 0.f));
  pattern->markComplete ();
  pattern->setChannel (channel);
  pattern->setEndAction (EndAction::Loop);
  pattern->setPlaybackLength (Measure{ 0, 1, 0 });
  return pattern;
}

/** One group on the floor, away from the clip's spot. */
FlightBodies
oneGroup ()
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { { -0.4f, 0.3f }, 1.f, 1 };
  return bodies;
}

double
degreesBetween (Pos const &a, Pos const &b)
{
  auto const dot = a.x () * b.x () + a.y () * b.y () + a.z () * b.z ();
  auto const lengths = a.distance () * b.distance ();
  auto const cosine = std::clamp (static_cast<double> (dot / lengths), -1., 1.);
  return std::acos (cosine) * 180. / juce::MathConstants<double>::pi;
}

/** What channels 0 and 1 looked like after the engine's tick: the handler is
 *  added after the engine's own, and a clock calls its handlers in order. */
struct Sample
{
  long long tick;
  FlightMode mode;
  Pos orbit; // channel 0, the one switched
  Pos clip;  // channel 1, the same clip left on CLIP
};

class TickRecorder
{
public:
  explicit TickRecorder (MotionEngine &engine)
  {
    _samples.reserve (16384);
    _handle = engine.getTempoClock ().scheduleEventHandlerAddition (
        [this, &engine] (Measure measure) {
          std::lock_guard<std::mutex> lock (_mutex);
          if (_samples.size () == _samples.capacity ())
            return;
          _samples.push_back (
              { Measure::convertToTicks (measure, engine.getBeatsPerBar ()),
                engine.getFlightMode (0), engine.getChannelPosition (0),
                engine.getChannelPosition (1) });
        },
        TempoClock::Event::Tick, TempoClock::Execution::TimerThread);
  }

  std::vector<Sample>
  samples ()
  {
    std::lock_guard<std::mutex> lock (_mutex);
    return _samples;
  }

  size_t
  count ()
  {
    std::lock_guard<std::mutex> lock (_mutex);
    return _samples.size ();
  }

private:
  std::mutex _mutex;
  std::vector<Sample> _samples;
  TempoClock::PointerT _handle;
};

/** An engine with one group on the floor and the standing clip playing on
 *  channels 0 and 1. Sends only through an OfflineBackend; `sending` is the
 *  one channel let through to it, the rest are in preview. */
struct Flight
{
  explicit Flight (index_t sending = 99,
                   std::function<void (Pattern &)> const &shape = {})
  {
    if (shape)
      {
        shape (*clip0);
        shape (*clip1);
      }
    auto owned = std::make_unique<OfflineBackend> ();
    backend = owned.get ();
    engine = std::make_unique<MotionEngine> (4, heightMap, std::move (owned));
    for (index_t channel = 0; channel < 4; ++channel)
      engine->setPreviewMode (channel, channel != sending);
    engine->setTempoBPM (bpm);
    engine->setFlightBodies (oneGroup ());
    engine->playPattern (clip0, Measure{});
    engine->playPattern (clip1, Measure{});
  }

  bool
  playing ()
  {
    return waitUntil ([&] {
      return clip0->getStatus () == Pattern::Status::Playing
             && clip1->getStatus () == Pattern::Status::Playing;
    });
  }

  HeightMapSphere heightMap;
  OfflineBackend *backend = nullptr;
  std::shared_ptr<Pattern> clip0 = clipStandingOn (0);
  std::shared_ptr<Pattern> clip1 = clipStandingOn (1);
  std::unique_ptr<MotionEngine> engine;
};

/** The first sample at or after `from` in `mode`, or -1. */
int
firstIn (std::vector<Sample> const &samples, FlightMode mode, size_t from = 0)
{
  for (auto i = from; i < samples.size (); ++i)
    if (samples[i].mode == mode)
      return static_cast<int> (i);
  return -1;
}

/** The widest step channel 0 took from one tick to the next. */
double
widestStep (std::vector<Sample> const &samples)
{
  auto widest = 0.;
  for (size_t i = 1; i < samples.size (); ++i)
    widest = std::max (widest,
                       degreesBetween (samples[i - 1].orbit, samples[i].orbit));
  return widest;
}

/** No tick went unsampled: a jump hidden in a gap would not be seen. */
bool
contiguous (std::vector<Sample> const &samples)
{
  for (size_t i = 1; i < samples.size (); ++i)
    if (samples[i].tick != samples[i - 1].tick + 1)
      return false;
  return true;
}
}

TEST (FlightEngine, AnOrbitChannelLeavesItsClipsSpot)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  TickRecorder recorder (*flight.engine);
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  EXPECT_EQ (flight.engine->getFlightMode (0), FlightMode::Orbit);
  EXPECT_EQ (flight.engine->getFlightMode (1), FlightMode::Clip);
  juce::Thread::sleep (1500);

  auto const samples = recorder.samples ();
  ASSERT_GT (samples.size (), 100u);

  auto lowest = 1e9, highest = -1e9, unwrapped = 0.;
  auto previous = static_cast<double> (samples.front ().orbit.azimuth ());
  for (auto const &sample : samples)
    {
      ASSERT_TRUE (sample.orbit.isValid ()) << "tick " << sample.tick;
      auto const azimuth = static_cast<double> (sample.orbit.azimuth ());
      unwrapped += std::remainder (azimuth - previous, 360.);
      previous = azimuth;
      lowest = std::min (lowest, unwrapped);
      highest = std::max (highest, unwrapped);

      EXPECT_EQ (sample.clip, samples.front ().clip)
          << "the CLIP channel moved at tick " << sample.tick;
    }
  EXPECT_GT (highest - lowest, 30.) << "the ORBIT channel did not fly";
}

TEST (FlightEngine, TheSwitchToOrbitDoesNotJump)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  TickRecorder recorder (*flight.engine);
  ASSERT_TRUE (waitUntil ([&] { return recorder.count () > 20; }));
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (500);

  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples)) << "ticks were missed while sampling";
  ASSERT_GE (firstIn (samples, FlightMode::Orbit), 1);
  EXPECT_LT (widestStep (samples), jumpDegrees);
  EXPECT_GT (degreesBetween (samples.front ().orbit, samples.back ().orbit),
             jumpDegrees)
      << "it never left the spot, so there was nothing to jump";
}

/** The switch on a clip whose figure is leant or swept: the channel is heard
 *  where playedPosition put it (band swept, plane turned), while the ship
 *  flies its clip's plain band. It glides in from the one to the other. */
void
expectNoJumpIntoOrbit (std::function<void (Pattern &)> const &shape)
{
  Flight flight (99, shape);
  ASSERT_TRUE (flight.playing ());
  TickRecorder recorder (*flight.engine);
  ASSERT_TRUE (waitUntil ([&] { return recorder.count () > 20; }));
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (500);

  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples)) << "ticks were missed while sampling";
  ASSERT_GE (firstIn (samples, FlightMode::Orbit), 1);
  EXPECT_LT (widestStep (samples), jumpDegrees);
}

TEST (FlightEngine, TheSwitchToOrbitDoesNotJumpOnALeantClip)
{
  // Acid Infinity's lean.
  expectNoJumpIntoOrbit (
      [] (Pattern &clip) { clip.setKnobSetting (Knob::Tilt, 1.f); });
}

TEST (FlightEngine, TheSwitchToOrbitDoesNotJumpOnASweptClip)
{
  // Tension Helix's lean, swept round by its roll.
  expectNoJumpIntoOrbit ([] (Pattern &clip) {
    clip.setKnobSetting (Knob::Tilt, 1.f);
    clip.setKnobSetting (Knob::RollSweep, 4.f);
  });
}

TEST (FlightEngine, BackToClipGlidesOverOneBeat)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (1000);
  TickRecorder recorder (*flight.engine);
  ASSERT_TRUE (waitUntil ([&] { return recorder.count () > 20; }));
  flight.engine->setFlightMode (0, FlightMode::Clip);

  auto const beat = static_cast<size_t> (TempoClock::getTicksPerBeat ());
  // The store can land after the engine read the mode and before the
  // recorder did: the engine may act on it one tick after it is sampled.
  auto const landed = beat + 10 + 1;
  ASSERT_TRUE (waitUntil ([&] {
    auto const samples = recorder.samples ();
    auto const back = firstIn (samples, FlightMode::Clip);
    return back >= 0 && samples.size () > static_cast<size_t> (back) + landed;
  }));

  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples)) << "ticks were missed while sampling";
  auto const back = static_cast<size_t> (firstIn (samples, FlightMode::Clip));
  ASSERT_GE (back, 1u);
  EXPECT_GT (degreesBetween (samples[back - 1].orbit, samples[back].clip), 10.)
      << "the ship was still on the clip's spot, so there was nothing to glide";

  EXPECT_LT (widestStep (samples), jumpDegrees);
  auto const arrived = samples[back + landed];
  EXPECT_NEAR (arrived.orbit.x (), arrived.clip.x (), 1e-3);
  EXPECT_NEAR (arrived.orbit.y (), arrived.clip.y (), 1e-3);
  EXPECT_NEAR (arrived.orbit.z (), arrived.clip.z (), 1e-3);

  // Not there at once: a glide, not a cut that happened to be small.
  auto const halfway = samples[back + beat / 2];
  EXPECT_GT (degreesBetween (halfway.orbit, halfway.clip), 1.);
}

TEST (FlightEngine, OrbitSendsNoMoreThanThePacerAllows)
{
  Flight flight (0);
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  auto const start = flight.engine->getChannelPosition (0);
  ASSERT_TRUE (waitUntil ([&] {
    return degreesBetween (flight.engine->getChannelPosition (0), start) > 1.;
  }));

  auto const sentBefore = flight.backend->positionsSent.load ();
  auto const before = juce::Time::getMillisecondCounterHiRes ();
  juce::Thread::sleep (2000);
  auto const sent = flight.backend->positionsSent.load () - sentBefore;
  auto const seconds = (juce::Time::getMillisecondCounterHiRes () - before) / 1000.;

  EXPECT_GT (sent, 0) << "nothing went out: the flight wrote no position";
  EXPECT_LE (sent, static_cast<int> (60. * seconds) + 2)
      << "over " << seconds << " s";
}

TEST (FlightEngine, AStoppedOrbitShipStandsStill)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  auto const start = flight.engine->getChannelPosition (0);
  ASSERT_TRUE (waitUntil ([&] {
    return degreesBetween (flight.engine->getChannelPosition (0), start) > 1.;
  }));

  flight.engine->stopPattern (flight.clip0, Measure{});
  ASSERT_TRUE (waitUntil (
      [&] { return flight.clip0->getStatus () == Pattern::Status::Idle; }));
  // One tick for the stop to reach the flight.
  juce::Thread::sleep (20);
  auto const stopped = flight.engine->getChannelPosition (0);
  juce::Thread::sleep (300);
  EXPECT_EQ (flight.engine->getChannelPosition (0), stopped)
      << "a ship whose clip is stopped kept flying";
  EXPECT_EQ (flight.engine->getFlightMode (0), FlightMode::Orbit);
}

TEST (FlightEngine, ATakePutsTheChannelBackOnClip)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setRecordingMode (MotionEngine::RecordingMode::OneShot);
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  ASSERT_EQ (flight.engine->getFlightMode (0), FlightMode::Orbit);

  auto take = std::make_shared<Pattern> ();
  take->setChannel (0);
  flight.engine->recordPattern (take, Measure{}, Measure{ 0, 1, 0 });

  ASSERT_TRUE (waitUntil ([&] { return take->wasRecording (); }))
      << "the take never started";
  EXPECT_EQ (flight.engine->getFlightMode (0), FlightMode::Clip)
      << "a take is the finger's path, not the physics";
  EXPECT_EQ (flight.engine->getFlightMode (1), FlightMode::Clip);
}
