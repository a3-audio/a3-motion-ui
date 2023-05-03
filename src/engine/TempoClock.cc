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

  struct FifoMessage
  {
    PointerT ptr;
    a3::TempoClock::Event event;
    a3::TempoClock::Notification notification;
  };

  ClockTimer (int numHandlersPreAllocated)
  {
    forEachHandlerType ([&] (auto event, auto notification, auto &container) {
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

    // fantasize a time for now
    auto time = std::chrono::steady_clock::now ().time_since_epoch ().count ();

    // execute or remove/erase callbacks
    forEachHandlerType ([&] (auto event, auto notification, auto &container) {
      auto it_erase_begin = std::remove_if (
          container.begin (), container.end (),
          [&] (const std::weak_ptr<std::function<a3::TempoClock::CallbackT> >
                   &func_ptr) {
            if (auto f = func_ptr.lock ())
              {
                // execute callback if pointer still valid
                (*f) (event, time);
                return false;
              }
            else // remove otherwise
              return true;
          });

      auto count = container.end () - it_erase_begin;
      container.erase (it_erase_begin, container.end ());
      if (count)
        juce::Logger::writeToLog ("erased elements: " + juce::String (count));
    });
  }

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
      for (auto notification : { a3::TempoClock::Notification::Sync,
                                 a3::TempoClock::Notification::Async })
        func (event, notification, handlers[{ event, notification }]);
  }

  void
  readFifoMessages ()
  {
    const auto scope = abstractFifo.read (abstractFifo.getNumReady ());

    if (scope.blockSize1 > 0)
      {
        juce::Logger::writeToLog ("Processing FIFO messages: "
                                  + juce::String (scope.blockSize1));
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

    auto numElements = scope.blockSize1 + scope.blockSize2;
    if (numElements)
      juce::Logger::writeToLog (
          "added " + juce::String (scope.blockSize1 + scope.blockSize2)
          + " elements");
  }

  void
  handleFifoMessage (SubmittedMessage &message)
  {
    auto &v = handlers[{ message.event, message.notification }];
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

  static constexpr int fifoSize = 32;
  juce::AbstractFifo abstractFifo{ fifoSize };
  std::array<SubmittedMessage, fifoSize> fifo;

  std::map<std::pair<a3::TempoClock::Event, a3::TempoClock::Notification>,
           ContainerT>
      handlers;

  a3::TempoClock::Measure measure;
};

namespace a3
{

TempoClock::TempoClock (TempoClock::Config const &config,
                        int const numHandlersPreAllocated)
    : config (config)
{
  timer = std::make_unique<ClockTimer> (numHandlersPreAllocated);
}

TempoClock::~TempoClock ()
{
  timer->stopTimer ();
}

TempoClock::PointerT
TempoClock::scheduleEventHandlerAddition (std::function<CallbackT> handler,
                                          Event event,
                                          Notification notification,
                                          bool waitForAck)
{
  auto guard = std::lock_guard<std::mutex> (mutexWriteFifo);

  auto ptr = std::make_shared<std::function<CallbackT> > (std::move (handler));

  auto future = timer->submitFifoMessage (
      { std::weak_ptr<std::function<CallbackT> > (ptr), event, notification });

  if (waitForAck)
    future.wait ();

  return ptr;
}

void
TempoClock::start ()
{
  timer->startTimer (config.timerIntervalMs);
  juce::Logger::writeToLog ("TempoClock: started");
}

void
TempoClock::pause ()
{
  timer->stopTimer ();
  juce::Logger::writeToLog ("TempoClock: paused");
}

void
TempoClock::stop ()
{
  timer->stopTimer (); // TODO and reset beat count
  juce::Logger::writeToLog ("TempoClock: stopped");
}

}
