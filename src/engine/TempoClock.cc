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

#include "../Config.hh"

class ClockTimer : public juce::HighResolutionTimer
{
public:
  using PointerT = std::weak_ptr<std::function<a3::TempoClock::CallbackT> >;
  using ContainerT = std::vector<PointerT>;
  using ClockT = std::chrono::high_resolution_clock;

  struct FifoMessage
  {
    PointerT ptr;
    a3::TempoClock::Event event;
    a3::TempoClock::Execution execution;
  };

  ClockTimer (a3::TempoClock::Config const &config) : config (config)
  {
    forEachHandlerType ([&] (auto event, auto execution, auto &container) {
      container.reserve (numHandlersPreAllocated);
    });
  }

  std::future<void>
  submitFifoMessage (FifoMessage const &message)
  {
    const auto scope = abstractFifo.write (1);

    jassert (scope.blockSize1 == 1);
    jassert (scope.blockSize2 == 0);
    fifo[scope.startIndex1] = SubmittedMessage{ message };

    return fifo[scope.startIndex1].acknowledge.get_future ();
  }

  void
  hiResTimerCallback () override
  {
    readFifoMessages ();
    advanceMeasure ();
  }

  std::atomic<bool> reset{ true };

private:
  struct SubmittedMessage : public FifoMessage
  {
    SubmittedMessage () : FifoMessage{} {}
    SubmittedMessage (FifoMessage const &fifoMessage)
        : FifoMessage{ fifoMessage }
    {
    }
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
        func (event, notification, handlers[{ event, notification }]);
  }

  void
  readFifoMessages ()
  {
    const auto scope = abstractFifo.read (abstractFifo.getNumReady ());

    if (scope.blockSize1 > 0)
      {
#ifdef DEBUG
        juce::Logger::writeToLog ("Processing FIFO messages: "
                                  + juce::String (scope.blockSize1));
#endif
        for (int idx = scope.startIndex1;
             idx < scope.startIndex1 + scope.blockSize1; ++idx)
          {
            handleFifoMessage (fifo[idx]);
          }
      }

    if (scope.blockSize2 > 0)
      for (int idx = scope.startIndex1;
           idx < scope.startIndex1 + scope.blockSize1; ++idx)
        {
          handleFifoMessage (fifo[idx]);
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
  handleFifoMessage (SubmittedMessage &message)
  {
    auto &v = handlers[{ message.event, message.execution }];
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
    auto ns_per_tick = config.ns_per_tick ();

    if (reset)
      {
        startTime = lastTick = now;
        measure = {};

        emitEvent (a3::TempoClock::Event::Tick);
        emitEvent (a3::TempoClock::Event::Beat);
        emitEvent (a3::TempoClock::Event::Bar);

        reset = false;
      }
    else
      {
        // catch up ticks
        while (std::chrono::duration_cast<std::chrono::nanoseconds> (
                   now - lastTick)
                   .count ()
               >= ns_per_tick)
          {
            lastTick += std::chrono::nanoseconds (ns_per_tick);
            measure.time_ns
                = std::chrono::duration_cast<std::chrono::nanoseconds> (
                      lastTick - startTime)
                      .count ();
            countTick ();
          }
      }
  }

  void
  countTick ()
  {
    ++measure.tick;
    if (measure.tick == config.ticksPerBeat)
      {
        measure.tick = 0;
        ++measure.beat;
        if (measure.beat == config.beatsPerBar)
          {
            measure.beat = 0;
            ++measure.bar;
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
        auto &container = handlers[{ event, execution }];

        auto it_erase_begin = std::remove_if (
            container.begin (), container.end (),
            [&] (const std::weak_ptr<std::function<a3::TempoClock::CallbackT> >
                     &func_ptr) {
              if (auto f = func_ptr.lock ()) // if pointer still valid
                {
                  switch (execution)
                    {
                    case a3::TempoClock::Execution::TimerThread:
                      (*f) (measure); // execute directly
                      break;
                    case a3::TempoClock::Execution::JuceMessageThread:
                      // NOTE: we copy the weak_ptr and check for
                      // validity again during the asynchronous
                      // execution in the message thread.
                      auto measureCopy{ measure };
                      juce::MessageManager::callAsync (
                          [func_ptr, measureCopy] () {
                            if (auto f = func_ptr.lock ())
                              (*f) (measureCopy);
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
  juce::AbstractFifo abstractFifo{ fifoSize };
  std::array<SubmittedMessage, fifoSize> fifo;

  std::map<std::pair<a3::TempoClock::Event, a3::TempoClock::Execution>,
           ContainerT>
      handlers;

  a3::TempoClock::Config const &config;

  ClockT::time_point startTime;
  ClockT::time_point lastTick;

  a3::TempoClock::Measure measure;
};

namespace a3
{

TempoClock::TempoClock ()
{
  timer = std::make_unique<ClockTimer> (config);
}

TempoClock::~TempoClock () {}

TempoClock::Config &
TempoClock::getConfig ()
{
  return config;
}

TempoClock::PointerT
TempoClock::scheduleEventHandlerAddition (std::function<CallbackT> handler,
                                          Event event, Execution execution,
                                          bool waitForAck)
{
  // makes sure that multiple threads/writers can schedule additions
  // without a race. should this be moved into the caller's responsibility?
  std::lock_guard<std::mutex> const guard{ mutexWriteFifo };

  // wrap the passed handler in a shared_ptr for lifetime management
  auto ptr = std::make_shared<std::function<CallbackT> > (std::move (handler));

  // submit to fifo queue and optionally wait for acknowledgement
  auto future = timer->submitFifoMessage (
      { std::weak_ptr<std::function<CallbackT> > (ptr), event, execution });
  if (waitForAck)
    future.wait ();

  // return shared_ptr to the user as callback for unregistration/deletion
  return ptr;
}

void
TempoClock::start ()
{
  auto now = ClockTimer::ClockT::now ();

  if (!timer->isTimerRunning ())
    {
      timer->reset = true;
      timer->startTimer (timerIntervalMs);
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
  if (timer->isTimerRunning ())
    {
      timer->stopTimer ();
#ifdef DEBUG
      juce::Logger::writeToLog ("TempoClock: stopped");
#endif
    }
}
}
