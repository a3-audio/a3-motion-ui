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

#include "TempoClock.hh"

#include <future>
#include <thread>

#include <JuceHeader.h>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <time.h>
#endif

#include <a3-motion-engine/Config.hh>
#include <a3-motion-engine/Measure.hh>
#include <a3-motion-engine/tempo/TempoEstimatorMean.hh>

// Dedicated real-time clock thread using clock_nanosleep for
// jitter-free tick scheduling.  Replaces juce::HighResolutionTimer
// which internally uses usleep and is susceptible to OS scheduling
// jitter.
class ClockTimer : public juce::AsyncUpdater
{
public:
  using PointerT = std::weak_ptr<std::function<a3::TempoClock::CallbackT> >;
  using ContainerT = std::vector<PointerT>;
  using ClockT = std::chrono::high_resolution_clock;

  struct Message
  {
    PointerT ptr;
    a3::TempoClock::Event event;
    a3::TempoClock::Execution execution;
  };

  ClockTimer (a3::TempoClock const &tempoClock) : _tempoClock (tempoClock)
  {
    forEachHandlerType ([&] (auto, auto, auto &container) {
      container.reserve (numHandlersPreAllocated);
    });
  }

  ~ClockTimer ()
  {
    stopThread ();
  }

  std::future<void>
  submitFifoMessage (Message const &message)
  {
    jassert (_abstractFifo.getFreeSpace () > 0);

    const auto scope = _abstractFifo.write (1);
    jassert (scope.blockSize1 == 1);
    jassert (scope.blockSize2 == 0);

    jassert (scope.startIndex1 >= 0);
    auto startIndex = static_cast<std::size_t> (scope.startIndex1);

    _fifo[startIndex] = SubmittedMessage{ message };

    return _fifo[startIndex].acknowledge.get_future ();
  }

  void
  startThread ()
  {
    if (_running.load ())
      return;
    _running.store (true);
    _thread = std::thread (&ClockTimer::threadFunc, this);
  }

  void
  stopThread ()
  {
    _running.store (false);
    if (_thread.joinable ())
      _thread.join ();
  }

  // Called on the JUCE message thread — dispatches pending events to
  // JuceMessageThread handlers without any heap allocation on the RT thread.
  void handleAsyncUpdate () override
  {
    // Atomically read and clear pending event flags
    unsigned flags = _pendingEvents.exchange (0, std::memory_order_acq_rel);
    if (!flags) return;

    // Read the latest measure snapshot (written by RT thread)
    a3::Measure m;
    {
      // SeqLock read — retry if writer was active
      unsigned seq;
      do {
        seq = _measureSeq.load (std::memory_order_acquire);
        m = _measureSnapshot;
        std::atomic_thread_fence (std::memory_order_acquire);
      } while (seq != _measureSeq.load (std::memory_order_acquire) || (seq & 1));
    }

    auto dispatch = [&] (a3::TempoClock::Event event) {
      auto &container = _handlers[handlerIndex (event,
                           a3::TempoClock::Execution::JuceMessageThread)];
      auto it = std::remove_if (
          container.begin (), container.end (),
          [&] (const PointerT &pw) {
            if (auto ps = pw.lock ())
              { (*ps) (m); return false; }
            return true;
          });
      container.erase (it, container.end ());
    };

    if (flags & kPendingTick) dispatch (a3::TempoClock::Event::Tick);
    if (flags & kPendingBeat) dispatch (a3::TempoClock::Event::Beat);
    if (flags & kPendingBar)  dispatch (a3::TempoClock::Event::Bar);
  }

  bool
  isRunning () const
  {
    return _running.load ();
  }

  std::atomic<bool> reset{ true };

private:
  void
  threadFunc ()
  {
    promoteToRealtimePriority ();

#ifdef __linux__
    // Use CLOCK_MONOTONIC + TIMER_ABSTIME for jitter-free scheduling.
    // We compute absolute wake-up times so accumulated drift is zero.
    struct timespec nextWake;
    clock_gettime (CLOCK_MONOTONIC, &nextWake);

    while (_running.load (std::memory_order_relaxed))
      {
        processFifoMessages ();
        advanceMeasure ();

        // Sleep until next 1ms boundary (absolute time — no drift)
        nextWake.tv_nsec += 1000000;  // 1 ms
        if (nextWake.tv_nsec >= 1000000000)
          {
            nextWake.tv_nsec -= 1000000000;
            nextWake.tv_sec += 1;
          }
        clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &nextWake, nullptr);
      }
#else
    // Fallback for non-Linux: use chrono sleep
    auto nextWake = std::chrono::steady_clock::now ();
    while (_running.load (std::memory_order_relaxed))
      {
        processFifoMessages ();
        advanceMeasure ();

        nextWake += std::chrono::microseconds (1000);  // 1 ms
        std::this_thread::sleep_until (nextWake);
      }
#endif
  }

  // Promote the clock thread to SCHED_FIFO once.
  // This guarantees the beatclock preempts GL/UI threads on RPi4.
  void promoteToRealtimePriority ()
  {
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = 70;  // high but leaves room for audio (usually 80+)
    int result = pthread_setschedparam (pthread_self (), SCHED_FIFO, &param);
    if (result != 0)
      {
        DBG ("Clock thread: could not set SCHED_FIFO (error " << result << ")");
      }
    else
      {
        DBG ("Clock thread promoted to SCHED_FIFO priority 70");
      }
#endif
  }
  struct SubmittedMessage : public Message
  {
    SubmittedMessage () : Message{} {}
    SubmittedMessage (Message const &fifoMessage) : Message{ fifoMessage } {}
    std::promise<void> acknowledge;
  };

  template <class FuncT>
  void
  forEachHandlerType (FuncT func)
  {
    for (auto event :
         { a3::TempoClock::Event::Tick, a3::TempoClock::Event::Beat,
           a3::TempoClock::Event::Bar })
      for (auto notification :
           { a3::TempoClock::Execution::TimerThread,
             a3::TempoClock::Execution::JuceMessageThread })
        func (event, notification, _handlers[handlerIndex (event, notification)]);
  }

  void
  processFifoMessages ()
  {
    auto const ready = _abstractFifo.getNumReady ();
    const auto scope = _abstractFifo.read (ready);

    jassert (scope.blockSize1 + scope.blockSize2 == ready);

    if (scope.blockSize1 > 0)
      {
#ifdef DEBUG
        juce::Logger::writeToLog ("Processing FIFO messages: "
                                  + juce::String (scope.blockSize1));
#endif
        for (int idx = scope.startIndex1;
             idx < scope.startIndex1 + scope.blockSize1; ++idx)
          {
            jassert (idx >= 0);
            handleMessage (_fifo[static_cast<std::size_t> (idx)]);
          }
      }

    if (scope.blockSize2 > 0)
      {
        for (int idx = scope.startIndex2;
             idx < scope.startIndex2 + scope.blockSize2; ++idx)
          {
            jassert (idx >= 0);
            handleMessage (_fifo[static_cast<std::size_t> (idx)]);
          }
      }

#ifdef DEBUG
    auto numElements = scope.blockSize1 + scope.blockSize2;
    if (numElements)
      juce::Logger::writeToLog (
          "added " + juce::String (scope.blockSize1 + scope.blockSize2)
          + " elements");
#endif
  }

  void
  handleMessage (SubmittedMessage &message)
  {
    auto &v = _handlers[handlerIndex (message.event, message.execution)];
    jassert (
        std::find_if (
            v.begin (), v.end (),
            [&] (const std::weak_ptr<std::function<a3::TempoClock::CallbackT> >
                     &func_ptr) {
              return func_ptr.lock () == message.ptr.lock ();
            })
        == v.end ());
    jassert (v.size () < v.capacity ());
    v.push_back (std::move (message.ptr));

    message.acknowledge.set_value ();
  }

  void
  advanceMeasure ()
  {
    auto now = ClockT::now ();
    auto nsPerTick = _tempoClock.getNanoSecondsPerTick ();

    if (reset)
      {
        _startTime = _lastTick = now;
        _measure = {};

        emitEvent (a3::TempoClock::Event::Tick);
        emitEvent (a3::TempoClock::Event::Beat);
        emitEvent (a3::TempoClock::Event::Bar);

        reset = false;
      }
    else
      {
        // catch up ticks
        while (std::chrono::duration_cast<std::chrono::nanoseconds> (
                   now - _lastTick)
                   .count ()
               >= nsPerTick)
          {
            _lastTick += std::chrono::nanoseconds (nsPerTick);
            countTick ();
          }
      }
  }

  void
  countTick ()
  {
    ++_measure.tick ();
    if (_measure.tick () == _tempoClock.getTicksPerBeat ())
      {
        _measure.tick () = 0;
        ++_measure.beat ();
        if (_measure.beat () == _tempoClock.getBeatsPerBar ())
          {
            _measure.beat () = 0;
            ++_measure.bar ();
            emitEvent (a3::TempoClock::Event::Bar);
          }
        emitEvent (a3::TempoClock::Event::Beat);
      }
    emitEvent (a3::TempoClock::Event::Tick);
  }

  void
  emitEvent (a3::TempoClock::Event event)
  {
    for (auto execution : { a3::TempoClock::Execution::TimerThread,
                            a3::TempoClock::Execution::JuceMessageThread })
      {
        if (execution == a3::TempoClock::Execution::JuceMessageThread)
          {
            // Throttle tick dispatch to every 4th tick
            if (event == a3::TempoClock::Event::Tick
                && (_measure.tick () % 4 != 0))
              continue;

            // Lock-free dispatch: write measure snapshot via SeqLock,
            // set pending event flag, and trigger async update.
            // Zero heap allocations on the RT thread.
            unsigned seq = _measureSeq.load (std::memory_order_relaxed);
            _measureSeq.store (seq + 1, std::memory_order_release); // odd = writing
            _measureSnapshot = _measure;
            _measureSeq.store (seq + 2, std::memory_order_release); // even = done

            unsigned bit = (event == a3::TempoClock::Event::Tick) ? kPendingTick
                         : (event == a3::TempoClock::Event::Beat) ? kPendingBeat
                         : kPendingBar;
            _pendingEvents.fetch_or (bit, std::memory_order_release);
            triggerAsyncUpdate ();
            continue;
          }

        // TimerThread: execute directly on this thread
        auto &container = _handlers[handlerIndex (event, execution)];

        auto it_erase_begin = std::remove_if (
            container.begin (), container.end (),
            [&] (const std::weak_ptr<std::function<a3::TempoClock::CallbackT> >
                     &ptrFuncWeak) {
              if (auto ptrFuncShared
                  = ptrFuncWeak.lock ())
                {
                  (*ptrFuncShared) (_measure);
                  return false;
                }
              else
                return true;
            });

        container.erase (it_erase_begin, container.end ());

#ifdef DEBUG
        auto count = container.end () - it_erase_begin;
        if (count)
          juce::Logger::writeToLog ("erased elements: "
                                    + juce::String (count));
#endif
      }
  }

  // Flat array indexed by (event * 2 + execution) — replaces std::map
  // for O(1) lookup with zero allocation on the timer thread.
  // Layout: [Tick/Timer, Tick/Msg, Beat/Timer, Beat/Msg, Bar/Timer, Bar/Msg]
  static constexpr std::size_t handlerIndex (a3::TempoClock::Event event,
                                              a3::TempoClock::Execution exec)
  {
    return static_cast<std::size_t> (event) * 2
         + static_cast<std::size_t> (exec);
  }
  static constexpr std::size_t kNumHandlerSlots = 6;  // 3 events × 2 executions

  static constexpr int numHandlersPreAllocated = 10;
  static constexpr int fifoSize = 32;
  juce::AbstractFifo _abstractFifo{ fifoSize };
  std::array<SubmittedMessage, fifoSize> _fifo;

  std::array<ContainerT, kNumHandlerSlots> _handlers;

  std::atomic<bool> _running{ false };
  std::thread _thread;

  a3::TempoClock const &_tempoClock;

  ClockT::time_point _startTime;
  ClockT::time_point _lastTick;

  a3::Measure _measure;

  // Lock-free RT → message-thread dispatch (no heap allocs)
  static constexpr unsigned kPendingTick = 1u;
  static constexpr unsigned kPendingBeat = 2u;
  static constexpr unsigned kPendingBar  = 4u;
  std::atomic<unsigned> _pendingEvents{ 0 };

  // SeqLock for Measure snapshot (RT writes, message-thread reads)
  std::atomic<unsigned> _measureSeq{ 0 };
  a3::Measure _measureSnapshot;
};

namespace a3
{

TempoClock::TempoClock ()
{
  _timer = std::make_unique<ClockTimer> (*this);
  _tempoEstimator = std::make_unique<TempoEstimatorMean> ();
}

TempoClock::~TempoClock () {}

TempoClock::PointerT
TempoClock::scheduleEventHandlerAddition (std::function<CallbackT> &&handler,
                                          Event event, Execution execution,
                                          bool waitForAck)
{
  // makes sure that multiple threads/writers can schedule additions
  // without a race. should this be moved into the caller's responsibility?
  std::lock_guard<std::mutex> const guard{ _mutexWriteFifo };

  // wrap the passed handler in a shared_ptr for lifetime management
  auto ptr = std::make_shared<std::function<CallbackT> > (std::move (handler));

  // submit to fifo queue and optionally wait for acknowledgement
  auto future = _timer->submitFifoMessage (
      { std::weak_ptr<std::function<CallbackT> > (ptr), event, execution });
  if (waitForAck)
    future.wait ();

  // return shared_ptr to the user as handle for unregistration/deletion
  return ptr;
}

float
TempoClock::getTempoBPM () const
{
  return _beatsPerMinute;
}

void
TempoClock::setTempoBPM (float tempoBPM)
{
  _beatsPerMinute = tempoBPM;
}

int
TempoClock::getBeatsPerBar () const
{
  return _beatsPerBar;
}

void
TempoClock::setBeatsPerBar (int beatsPerBar)
{
  _beatsPerBar = beatsPerBar;
}

int64_t
TempoClock::getNanoSecondsPerTick () const
{
  return int64_t (60) * 1000000000 / double (_beatsPerMinute) / ticksPerBeat;
}

TempoClock::TapResult
TempoClock::tap (juce::int64 timeMicros)
{
  auto const result = _tempoEstimator->tap (timeMicros);

  if (result == TempoEstimator::TapResult::FirstTap)
    {
      // First tap after timeout - reset beat to 1
      reset ();
      return TapResult::FirstTap;
    }

  if (result == TempoEstimator::TapResult::TempoAvailable)
    {
      setTempoBPM (_tempoEstimator->getTempoBPM ());
      return TapResult::TempoAvailable;
      // TODO: send OSC tempo via async command queue
      // we will have to indirect this through the MotionEngine
    }
  return TapResult::TempoNotAvailable;
}

void
TempoClock::start ()
{
  if (!_timer->isRunning ())
    {
      _timer->reset = true;
      _timer->startThread ();
#ifdef DEBUG
      juce::Logger::writeToLog ("TempoClock: started (dedicated thread)");
#endif
    }
#ifdef DEBUG
  else
    {
      juce::Logger::writeToLog ("TempoClock: was already started!");
    }
#endif
}

void
TempoClock::stop ()
{
  if (_timer->isRunning ())
    {
      _timer->stopThread ();
#ifdef DEBUG
      juce::Logger::writeToLog ("TempoClock: stopped");
#endif
    }
}

void
TempoClock::reset ()
{
  _timer->reset = true;
}

Measure
TempoClock::nextDownBeat (Measure const &measure)
{
  auto downbeat = measure;

  if (downbeat.beat () != 0 || downbeat.tick () != 0)
    {
      ++downbeat.bar ();
      downbeat.beat () = 0;
      downbeat.tick () = 0;
    }

  return downbeat;
}

}
