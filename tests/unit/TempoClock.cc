#include <gtest/gtest.h>

#include <engine/TempoClock.hh>
#include <util/ScopedTimer.hh>

using namespace a3;

TEST (TempoClock, TimingSyncAsync)
{
  TempoClock tempoClock;
  // tempoClock.start ();

  ScopedTimer<>::TimingResults results;

  constexpr auto numTries = 10;
  for (int i = 0; i < numTries; ++i)
    { // testing
      ScopedTimer<> t{ results, "waiting" };
      auto ptr = tempoClock.scheduleEventHandlerAddition (
          [] (auto event, auto time) {
            juce::Logger::writeToLog ("sync: " + juce::String (time));
          },
          TempoClock::Event::Beat, TempoClock::Notification::Sync, true);
    }

  tempoClock.start ();
}

// for (int i = 0; i < numTries; ++i)
//   {
//     ScopedTimer<> t{ results, "not waiting" };

//     auto callback = [] (auto event, auto time) {
//       // juce::Logger::writeToLog ("async: " + juce::String (time));
//     };

//     auto address = static_cast<void *> (&callback);
//     // juce::Logger::writeToLog (juce::String (address));
//     std::cout << address << std::endl;

//     auto ptr = tempoClock.scheduleEventHandlerAddition (
//         callback, TempoClock::Event::Beat, TempoClock::Notification::Async,
//         false);
//   }

// for (auto const &t : results.getTimings ())
//   {
//     juce::Logger::writeToLog (juce::String (t.tag) + ": "
//                               + juce::String (t.duration.count ()));
//   }
