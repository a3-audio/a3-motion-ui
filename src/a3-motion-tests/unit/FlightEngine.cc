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

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/PilotOrder.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>
#include <a3-motion-engine/flight/FlightField.hh>
#include <a3-motion-engine/flight/PilotGames.hh>
#include <a3-motion-engine/preview/MusicCue.hh>

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <optional>
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

  /** The tick last sampled, -1 before the first. */
  long long
  lastTick ()
  {
    std::lock_guard<std::mutex> lock (_mutex);
    return _samples.empty () ? -1 : _samples.back ().tick;
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

// The breath, as the engine flies it: off until asked for, and then the ORBIT
// channel stands still through the last beat of every bar.

// The maintainer's verdict after the device A/B (2026-10-08): the breath is
// the default, the key switches it off for the session.
TEST (FlightEngine, TheShipsBreatheFromTheStart)
{
  Flight flight;
  EXPECT_TRUE (flight.engine->getFlightBreath ());
  flight.engine->setFlightBreath (false);
  EXPECT_FALSE (flight.engine->getFlightBreath ());
  flight.engine->setFlightBreath (true);
  EXPECT_TRUE (flight.engine->getFlightBreath ());
}

TEST (FlightEngine, ABreathingOrbitChannelStandsStillOnBeatFour)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightBreath (true);
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  // Past the one-beat glide in, which moves the channel on its own.
  juce::Thread::sleep (600);
  TickRecorder recorder (*flight.engine);
  juce::Thread::sleep (2200); // two bars and more at 240 BPM

  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples)) << "ticks were missed while sampling";
  auto const ticksPerBeat = TempoClock::getTicksPerBeat ();
  auto const ticksPerBar = ticksPerBeat * flight.engine->getBeatsPerBar ();
  auto held = 0, moved = 0;
  for (size_t i = 1; i < samples.size (); ++i)
    {
      auto const inBar = samples[i].tick % ticksPerBar;
      auto const step = degreesBetween (samples[i - 1].orbit, samples[i].orbit);
      if (inBar >= ticksPerBar - ticksPerBeat) // every tick of beat 4
        {
          EXPECT_EQ (samples[i].orbit, samples[i - 1].orbit)
              << "moved on beat 4 at tick " << samples[i].tick;
          ++held;
        }
      else if (step > 0.)
        ++moved;
    }
  EXPECT_GT (held, 0) << "no beat 4 was sampled";
  EXPECT_GT (moved, 0) << "it never flew between the stops";
}

// -- An action on a flying ship ----------------------------------------------

namespace
{
/** `motion` fired at `channel` with the clip's own settings, its accent held:
 *  nothing changes on the clip, only what the ship is asked. */
void
fireAt (Flight &flight, index_t channel, FlightMotion const &motion,
        std::function<void (ClipSettings &)> const &change = {})
{
  auto const &clip = channel == 0 ? flight.clip0 : flight.clip1;
  auto settings = clipSettingsFrom (*clip);
  settings.actMode = ActMode::Hold;
  if (change)
    change (settings);
  flight.engine->setChannelAction (channel, settings, motion);
  flight.engine->setChannelAccentHeld (channel, true, clip);
}

/** How far channel 0 went round the room in `ms`, every tick's step counted. */
double
turnedDegrees (MotionEngine &engine, int ms)
{
  TickRecorder recorder (engine);
  juce::Thread::sleep (ms);
  auto const samples = recorder.samples ();
  auto travel = 0.;
  for (size_t i = 1; i < samples.size (); ++i)
    travel += std::abs (std::remainder (
        static_cast<double> (samples[i].orbit.azimuth ()
                             - samples[i - 1].orbit.azimuth ()),
        360.));
  return travel;
}

double
capPerTick ()
{
  return static_cast<double> (FlightTuning{}.angularCapDegreesPerBeat)
         / TempoClock::getTicksPerBeat ();
}

FlightMotion
spinOf (int step)
{
  FlightMotion motion;
  motion.spin = step;
  return motion;
}
}

TEST (FlightEngine, AnActionsSpinTurnsAnOrbitShipFaster)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (300);
  auto const plain = turnedDegrees (*flight.engine, 1500);
  fireAt (flight, 0, spinOf (6)); // a lap a bar
  juce::Thread::sleep (300);      // the beat it takes to take hold, at 240 BPM
  auto const driven = turnedDegrees (*flight.engine, 1500);
  EXPECT_GT (driven, 2. * plain) << "plain " << plain << ", driven " << driven;
}

TEST (FlightEngine, AnActionNeverJumpsTheShip)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (300);
  TickRecorder recorder (*flight.engine);
  FlightMotion leant = spinOf (6);
  leant.tilt = 1.f;
  fireAt (flight, 0, leant);
  juce::Thread::sleep (600);
  flight.engine->setChannelAccentHeld (0, false, nullptr);
  juce::Thread::sleep (1200); // the fall, the restore, the beat to let go
  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples)) << "ticks were missed while sampling";
  EXPECT_LE (widestStep (samples), capPerTick () * 1.05);
}

TEST (FlightEngine, EvenAWildActionStaysUnderTheAngularCap)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (300);
  FlightMotion wild = spinOf (8);
  wild.speedLog2 = -7;
  wild.tilt = 2.f;
  wild.tiltSweep = 8;
  wild.rollSweep = -8;
  wild.sway = 8;
  wild.swell = 8;
  TickRecorder recorder (*flight.engine);
  fireAt (flight, 0, wild);
  juce::Thread::sleep (1500);
  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples));
  EXPECT_LE (widestStep (samples), capPerTick () * 1.05);
}

// A flying ship ignores an action's elevation keys: it flies its clip's own
// band. The clip's band (base at the pole, reach 0.5) keeps the ship above
// the ear; an action moving the base to the south pole must not take it
// there.
TEST (FlightEngine, AnActionsElevationKeysDoNotMoveAFlyingShip)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (300);
  TickRecorder recorder (*flight.engine);
  fireAt (flight, 0, FlightMotion{}, [] (ClipSettings &s) {
    s.elevationBase = 1.f;
    s.reach = -0.5f;
  });
  juce::Thread::sleep (1000);
  for (auto const &sample : recorder.samples ())
    ASSERT_GT (sample.orbit.elevation (), -1.f) << "tick " << sample.tick;
}

TEST (FlightEngine, AClipShipKeepsItsClip)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  TickRecorder recorder (*flight.engine);
  fireAt (flight, 1, spinOf (8));
  juce::Thread::sleep (800);
  auto const samples = recorder.samples ();
  ASSERT_GT (samples.size (), 50u);
  for (auto const &sample : samples)
    ASSERT_EQ (sample.clip, samples.front ().clip) << "tick " << sample.tick;
}

// Switched to ORBIT while an action's accent runs, the ship launches undriven
// and eases into the running action.
TEST (FlightEngine, ASwitchToOrbitDuringAnActionDoesNotJump)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  TickRecorder recorder (*flight.engine);
  ASSERT_TRUE (waitUntil ([&] { return recorder.count () > 20; }));
  FlightMotion leant = spinOf (7);
  leant.roll = 1.f;
  fireAt (flight, 0, leant);
  juce::Thread::sleep (200);
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (800);
  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples));
  ASSERT_GE (firstIn (samples, FlightMode::Orbit), 1);
  EXPECT_LT (widestStep (samples), jumpDegrees);
}

// The latest press wins, and the lean changes over without a jump.
TEST (FlightEngine, ASecondPressChangesOverUnderTheCap)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  juce::Thread::sleep (300);
  TickRecorder recorder (*flight.engine);
  FlightMotion first;
  first.tilt = 2.f;
  fireAt (flight, 0, first);
  juce::Thread::sleep (500);
  FlightMotion second;
  second.roll = -2.f;
  fireAt (flight, 0, second);
  juce::Thread::sleep (800);
  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples));
  EXPECT_LE (widestStep (samples), capPerTick () * 1.05);
}

// -- Games --------------------------------------------------------------------
// The board on the clock thread: a request is taken on the next tick and
// played; a game borrows a ship without touching the DJ's mode, and gives it
// back when it ends.

namespace
{
PilotOrder
playing (PilotGame game, PilotRecruit with = PilotRecruit::Self)
{
  PilotOrder order;
  order.game = game;
  order.with = with;
  return order;
}

/** The bar the clock is in, read off a tick it has just run. */
long long
barNow (MotionEngine &engine)
{
  TickRecorder recorder (engine);
  if (!waitUntil ([&] { return recorder.count () > 0; }))
    return 0;
  auto const ticksPerBar
      = static_cast<long long> (TempoClock::getTicksPerBeat ()) * engine.getBeatsPerBar ();
  return recorder.samples ().back ().tick / ticksPerBar;
}

MusicCue
headingFor (MusicSection now, MusicSection next, long long changeBar)
{
  MusicCue cue;
  cue.section = now;
  cue.next = next;
  cue.changeBar = changeBar;
  cue.energy = 0.5f;
  return cue;
}

/** The widest step channel 1 (Sample::clip) took from one tick to the next. */
double
widestStepOfChannelOne (std::vector<Sample> const &samples)
{
  auto widest = 0.;
  for (size_t i = 1; i < samples.size (); ++i)
    widest = std::max (widest, degreesBetween (samples[i - 1].clip, samples[i].clip));
  return widest;
}
}

TEST (FlightEngine, AGameRequestStartsAGameOnItsShips)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  flight.engine->requestGame (0, playing (PilotGame::Formation, PilotRecruit::All));
  ASSERT_TRUE (waitUntil ([&] {
    auto const leader = flight.engine->gameOf (0);
    auto const borrowed = flight.engine->gameOf (1);
    return leader && leader->game == PilotGame::Formation && borrowed
           && borrowed->game == PilotGame::Formation;
  }));
  EXPECT_FALSE (flight.engine->pendingGame (0).has_value ()) << "taken, not waiting";
  EXPECT_FALSE (flight.engine->gameOf (1)->byPilot);
  EXPECT_EQ (flight.engine->gameOf (1)->leader, 0);
  EXPECT_EQ (flight.engine->getFlightMode (1), FlightMode::Clip)
      << "a game never writes the DJ's mode";
  EXPECT_FALSE (flight.engine->gameOf (2).has_value ()) << "no clip runs on channel 3";

  flight.engine->requestGame (0, playing (PilotGame::None));
  EXPECT_TRUE (waitUntil ([&] {
    return !flight.engine->gameOf (0).has_value () && !flight.engine->gameOf (1).has_value ();
  }));
}

TEST (FlightEngine, AGameOnAChannelThatCannotFlyStartsNothing)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->requestGame (2, playing (PilotGame::FakeOut)); // no clip runs on channel 3
  juce::Thread::sleep (200);
  EXPECT_FALSE (flight.engine->gameOf (2).has_value ());
  EXPECT_FALSE (flight.engine->pendingGame (2).has_value ()) << "dropped, not left to start later";
  EXPECT_FALSE (flight.engine->gameOf (0).has_value ());
}

TEST (FlightEngine, ABorrowedClipShipGlidesBackToItsClipWhenTheGameEnds)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  auto const onItsClip = flight.engine->getChannelPosition (1);
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  flight.engine->setMusicCue (
      headingFor (MusicSection::Build, MusicSection::Drop, barNow (*flight.engine) + 2));
  flight.engine->requestGame (0, playing (PilotGame::Formation, PilotRecruit::All));
  ASSERT_TRUE (waitUntil ([&] { return flight.engine->gameOf (1).has_value (); }));
  ASSERT_TRUE (waitUntil ([&] { return !flight.engine->gameOf (1).has_value (); }, 6000));
  juce::Thread::sleep (600); // the glide back is a beat, 250 ms at 240 BPM
  EXPECT_LT (degreesBetween (flight.engine->getChannelPosition (1), onItsClip), 1.);
}

TEST (FlightEngine, TheDjsPagePressTakesAShipOutOfItsGame)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  flight.engine->requestGame (0, playing (PilotGame::Formation, PilotRecruit::All));
  ASSERT_TRUE (waitUntil ([&] { return flight.engine->gameOf (1).has_value (); }));
  flight.engine->setFlightMode (1, FlightMode::Orbit);
  EXPECT_TRUE (waitUntil ([&] { return !flight.engine->gameOf (1).has_value (); }));
  EXPECT_TRUE (flight.engine->gameOf (0).has_value ()) << "the rest play on";
}

TEST (FlightEngine, CallingOffEndsEveryGameAndDropsWhatWaits)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  flight.engine->requestGame (0, playing (PilotGame::Formation, PilotRecruit::Self));
  ASSERT_TRUE (waitUntil ([&] { return flight.engine->gameOf (0).has_value (); }));
  flight.engine->requestGame (1, playing (PilotGame::HideAndSeek));
  flight.engine->callOffGames ();
  EXPECT_TRUE (waitUntil ([&] {
    return !flight.engine->gameOf (0).has_value () && !flight.engine->gameOf (1).has_value ()
           && !flight.engine->pendingGame (1).has_value ();
  }));
  juce::Thread::sleep (200);
  EXPECT_FALSE (flight.engine->gameOf (1).has_value ())
      << "a request queued before the call-off does not start after it";
}

TEST (FlightEngine, AtFlyAPilotStartsAGameOfItsOwnAndStopsBelowIt)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  flight.engine->setMusicCue (
      headingFor (MusicSection::Breakdown, MusicSection::Build, barNow (*flight.engine) + 6));
  flight.engine->setPilotLevel (PilotLevel::Fly);
  ASSERT_TRUE (waitUntil (
      [&] {
        auto const game = flight.engine->gameOf (0);
        return game && game->byPilot && game->game == PilotGame::HideAndSeek;
      },
      3000));
  EXPECT_FALSE (flight.engine->gameOf (1).has_value ()) << "a pilot takes no ship off its clip";
  flight.engine->setPilotLevel (PilotLevel::Hint);
  EXPECT_TRUE (waitUntil ([&] { return !flight.engine->gameOf (0).has_value (); }));
}

TEST (FlightEngine, AGameNeverTurnsAHeardShipFasterThanTheCap)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  flight.engine->setFlightMode (1, FlightMode::Orbit);
  juce::Thread::sleep (300);
  flight.engine->setMusicCue (
      headingFor (MusicSection::Breakdown, MusicSection::Build, barNow (*flight.engine) + 4));
  TickRecorder recorder (*flight.engine);
  // Hide & seek for two flying ships: the slip, the run across the middle and
  // the hand-back to their own flight, all in one recording. (A ship borrowed
  // from its clip glides back over the clip's own one-beat handover, which is
  // not this limit's: ABorrowedClipShipGlidesBackToItsClipWhenTheGameEnds.)
  flight.engine->requestGame (0, playing (PilotGame::HideAndSeek, PilotRecruit::Nearest));
  juce::Thread::sleep (5500); // four bars and the one after, at 240 BPM
  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples)) << "ticks were missed while sampling";
  EXPECT_LE (widestStep (samples), capPerTick () * 1.05);
  EXPECT_LE (widestStepOfChannelOne (samples), capPerTick () * 1.05);
}

// The group a game is played against does not tug at the ship in it: the
// figure already stands for it, as an escort's circle does for the escorted
// group. The world spares the body the orders name, so the engine has to
// name it.

namespace
{
/** One heavy group, the only body on the floor. */
FlightBodies
aHeavyGroupAt (Vec2 at)
{
  FlightBodies bodies;
  bodies.count = 1;
  bodies.body[0] = { at, 3.f, 1 };
  return bodies;
}

/** Where channel 0 stands on the floor, read back through its clip's band. */
Vec2
floorPointOf (Flight &flight, Pos const &heard)
{
  auto const p = flight.heightMap.mapTo2D (heard, flight.clip0->getElevationParams ());
  return { p.x (), p.y () };
}

/** The sample of `tick`, or none when it was not recorded. */
std::optional<Sample>
sampleAt (std::vector<Sample> const &samples, long long tick)
{
  for (auto const &sample : samples)
    if (sample.tick == tick)
      return sample;
  return std::nullopt;
}
}

TEST (FlightEngine, AShipInAGameIsSparedItsTargetsPull)
{
  Flight flight;
  ASSERT_TRUE (flight.playing ());
  // A formation of one lines up at 0.4 from the middle on its group's side
  // and bursts back across the middle on the 1 -- the one beat a bar the
  // groups pull. A heavy group 0.1 beyond the line pulls as hard as a ship
  // can steer: pulled, the ship stays put for the beat; spared, it leaves.
  flight.engine->setFlightBodies (aHeavyGroupAt ({ 0.5f, 0.f }));
  flight.engine->setFlightMode (0, FlightMode::Orbit);
  auto const climaxBar = barNow (*flight.engine) + 3;
  flight.engine->setMusicCue (headingFor (MusicSection::Build, MusicSection::Drop, climaxBar));
  TickRecorder recorder (*flight.engine);
  flight.engine->requestGame (0, playing (PilotGame::Formation));
  ASSERT_TRUE (waitUntil ([&] { return flight.engine->gameOf (0).has_value (); }));
  EXPECT_EQ (flight.engine->gameOf (0)->target, 1);

  auto const ticksPerBeat = static_cast<long long> (TempoClock::getTicksPerBeat ());
  auto const burstTick = climaxBar * ticksPerBeat * flight.engine->getBeatsPerBar ();
  ASSERT_TRUE (waitUntil ([&] { return recorder.lastTick () > burstTick + ticksPerBeat; }, 6000));
  auto const samples = recorder.samples ();
  ASSERT_TRUE (contiguous (samples)) << "ticks were missed while sampling";
  auto const onTheOne = sampleAt (samples, burstTick);
  auto const aBeatLater = sampleAt (samples, burstTick + ticksPerBeat);
  ASSERT_TRUE (onTheOne && aBeatLater);
  auto const place = floorPointOf (flight, onTheOne->orbit);
  EXPECT_NEAR (place.x, 0.4f, 0.1f) << "not lined up on the group's side";
  EXPECT_GT (floorPointOf (flight, aBeatLater->orbit).getDistanceFrom (place), 0.25f)
      << "held at its place by the group's pull through the burst beat";
}
