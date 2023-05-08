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

#pragma once

#include <map>
#include <mutex>

#include <JuceHeader.h>

#include "../Config.hh"

class ClockTimer;

namespace a3
{

/*
 * TempoClock drives the motion engine for playback/recording and OSC
 * communication.
 *
 * The JUCE HighResolutionTimer uses a high priority thread with a
 * millisecond timer resolution. We'll try it and see if the
 * performance is sufficient. Otherwise we might choose to implement
 * the timer ourselves with a realtime thread and a higher resolution.
 *
 * TODO: with the current implementation, a callback's std::function
 * object could be deallocated on the realtime thread. This happens
 * when the user thread's shared_ptr lifetime ends while the weak_ptr
 * is currently locked. If this should turn out to noticeably mess
 * with our clock accuracy, we can perform explicit garbage collection
 * on an additional low-priority thread as suggested in the 2015
 * CppCon talk by Timur Doumler: https://youtu.be/boPEO2auJj4?t=2817
 */
class TempoClock
{
public:
  // ticksPerBeat equal PPQN (pulses per quarter note). MIDI uses 24,
  // modern sequencers up to 960 (Wikipedia) to capture timing
  // nuances.
  struct Config
  {
    int
    ns_per_tick () const
    {
      return uint64_t (60) * 1000000000 / ticksPerBeat / beatsPerMinute;
    }

    std::atomic<int> beatsPerMinute{ 90 };
    std::atomic<int> beatsPerBar{ 4 };

    static constexpr int ticksPerBeat = 128;
    static_assert (std::atomic<int>::is_always_lock_free);
  };

  struct Measure
  {
    Measure () : time_ns (0), tick (0), beat (0), bar (0) {}
    uint64_t time_ns;
    int tick;
    int beat;
    int bar;
  };

  enum class Execution
  {
    JuceMessageThread,
    TimerThread
  };
  enum class Event
  {
    Bar,
    Beat,
    Tick
  };

  using CallbackT = void (a3::TempoClock::Measure);
  using PointerT = std::shared_ptr<std::function<CallbackT> >;

  TempoClock ();
  ~TempoClock ();

  Config &getConfig ();

  /* Schedule addition of an event handler. The function returns a
   shared_ptr to the message handler, which has to be kept alive by
   the caller. The callback is deleted when the shared_ptr is
   invalidated or goes out of scope. waitForAck can be passed to wait
   until the handler has been picked up by the high priority thread.
   */
  PointerT scheduleEventHandlerAddition (std::function<CallbackT> handler,
                                         Event event, Execution execution,
                                         bool waitForAck = false);

  void start ();
  // void pause (); // needs more thought
  void stop ();

private:
  Config config;

  std::unique_ptr<ClockTimer> timer;
  static constexpr int timerIntervalMs = 1;

  // We use a single-producer single-consumer lock-less ring buffer to
  // forward add/delete requests of event handlers to the timer
  // thread. To extend this to multiple callers we lock on the
  // producer side.
  std::mutex mutexWriteFifo;
};

}
