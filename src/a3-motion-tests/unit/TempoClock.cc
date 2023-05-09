#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/TempoClock.hh>
#include <a3-motion-engine/util/Timing.hh>

using namespace a3;

TEST (TempoClock, TimingSyncAsync)
{
  TempoClock tempoClock;
  tempoClock.start ();

  Timings timings;

  {
    ScopedTimer<> t{ timings, "waiting" };
    auto ptr = tempoClock.scheduleEventHandlerAddition (
        [] (auto) {
          // juce::Logger::writeToLog ("sync: " + juce::String (time));
        },
        TempoClock::Event::Beat, TempoClock::Execution::TimerThread, true);
  }

  tempoClock.stop ();
}
