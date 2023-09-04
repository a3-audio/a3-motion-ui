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

#include <JuceHeader.h>

#include <a3-motion-engine/Config.hh>
#include <a3-motion-engine/tempo/Measure.hh>
#include <a3-motion-engine/tempo/TempoEstimatorMean.hh>

// TODO define this in anonymous namespace or move to internal
// implementation file?
class ClockTimer : public juce::HighResolutionTimer
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

  ClockTimer (a3::TempoClock::Config const &config) : _config (config)
  {
    forEachHandlerType ([&] (auto, auto, auto &container) {
      container.reserve (numHandlersPreAllocated);
    });
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
  hiResTimerCallback () override
  {
    processFifoMessages ();
    advanceMeasure ();
  }

  std::atomic<bool> reset{ true };

private:
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
        func (event, notification, _handlers[{ event, notification }]);
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
    auto &v = _handlers[{ message.event, message.execution }];
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
    auto ns_per_tick = _config.nsPerTick ();

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
               >= ns_per_tick)
          {
            _lastTick += std::chrono::nanoseconds (ns_per_tick);
            countTick ();
          }
      }
  }

  void
  countTick ()
  {
    ++_measure.tick ();
    if (_measure.tick () == _config.ticksPerBeat)
      {
        _measure.tick () = 0;
        ++_measure.beat ();
        if (_measure.beat () == _config.beatsPerBar)
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
        auto &container = _handlers[{ event, execution }];

        auto it_erase_begin = std::remove_if (
            container.begin (), container.end (),
            [&] (const std::weak_ptr<std::function<a3::TempoClock::CallbackT> >
                     &ptrFuncWeak) {
              if (auto ptrFuncShared
                  = ptrFuncWeak.lock ()) // if pointer still valid
                {
                  switch (execution)
                    {
                    case a3::TempoClock::Execution::TimerThread:
                      (*ptrFuncShared) (_measure); // execute directly
                      break;
                    case a3::TempoClock::Execution::JuceMessageThread:
                      // NOTE: we copy the weak_ptr and check for
                      // validity again during the asynchronous
                      // execution in the message thread.
                      auto measureCopy{ _measure };
                      juce::MessageManager::callAsync ([ptrFuncWeak,
                                                        measureCopy] () {
                        if (auto ptrFuncSharedMessage = ptrFuncWeak.lock ())
                          (*ptrFuncSharedMessage) (measureCopy);
                      });
                      break;
                    }
                  return false;
                }
              else // remove otherwise
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

  static constexpr int numHandlersPreAllocated = 10;
  static constexpr int fifoSize = 32;
  juce::AbstractFifo _abstractFifo{ fifoSize };
  std::array<SubmittedMessage, fifoSize> _fifo;

  std::map<std::pair<a3::TempoClock::Event, a3::TempoClock::Execution>,
           ContainerT>
      _handlers;

  a3::TempoClock::Config const &_config;

  ClockT::time_point _startTime;
  ClockT::time_point _lastTick;

  a3::Measure _measure;
};

namespace a3
{

TempoClock::TempoClock ()
{
  _timer = std::make_unique<ClockTimer> (_config);
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
  return _config.beatsPerMinute;
}

void
TempoClock::setTempoBPM (float tempoBPM)
{
  _config.beatsPerMinute = tempoBPM;
}

TempoClock::TapResult
TempoClock::tap (juce::int64 timeMicros)
{
  if (_tempoEstimator->tap (timeMicros)
      == TempoEstimator::TapResult::TempoAvailable)
    {
      setTempoBPM (_tempoEstimator->getTempoBPM ());
      return TapResult::TempoAvailable;
      // TODO: send OSC tempo via async command queue
    }
  return TapResult::TempoNotAvailable;
}

void
TempoClock::start ()
{
  if (!_timer->isTimerRunning ())
    {
      _timer->reset = true;
      _timer->startTimer (timerIntervalMs);
#ifdef DEBUG
      juce::Logger::writeToLog ("TempoClock: started");
#endif
    }
#ifdef DEBUG
  else
    {
      juce::Logger::writeToLog ("TempoClock: was already started!");
    }
#endif
}

// void
// TempoClock::pause ()
// {
//   timer->stopTimer ();
// #ifdef DEBUG
//   juce::Logger::writeToLog ("TempoClock: paused");
// #endif
// }

void
TempoClock::stop ()
{
  if (_timer->isTimerRunning ())
    {
      _timer->stopTimer ();
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
