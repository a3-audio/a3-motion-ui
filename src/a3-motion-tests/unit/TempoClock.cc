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
