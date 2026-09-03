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

#include <a3-motion-engine/tempo/TempoClock.hh>
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

// The first tap after a pause is supposed to reset the beat to 1. Reported as
// not happening in INT clock mode, and the reset runs on the clock's own
// thread, so this watches what the Beat handler is actually handed rather than
// what the tap returns.
TEST (TempoClock, FirstTapResetsTheBeat)
{
  TempoClock tempoClock;
  tempoClock.start ();

  std::atomic<int> lastBeat{ -1 };
  std::atomic<int> beatCount{ 0 };

  auto handle = tempoClock.scheduleEventHandlerAddition (
      [&lastBeat, &beatCount] (Measure measure) {
        lastBeat = static_cast<int> (measure.beat ());
        ++beatCount;
      },
      TempoClock::Event::Beat, TempoClock::Execution::TimerThread, true);

  // Let it run a while so the beat is somewhere other than 1 when we tap.
  auto const beatsBeforeTap = beatCount.load ();
  for (int i = 0; i < 200 && beatCount.load () < beatsBeforeTap + 2; ++i)
    juce::Thread::sleep (10);

  auto const now = juce::Time::getHighResolutionTicks ();
  auto const micros = static_cast<juce::int64> (
      static_cast<double> (now)
      / static_cast<double> (juce::Time::getHighResolutionTicksPerSecond ())
      * 1'000'000.0);

  auto const result = tempoClock.tap (micros);
  EXPECT_EQ (result, TempoClock::TapResult::FirstTap)
      << "a lone tap has to count as the first one";

  auto const countAtTap = beatCount.load ();
  for (int i = 0; i < 100 && beatCount.load () == countAtTap; ++i)
    juce::Thread::sleep (5);

  EXPECT_GT (beatCount.load (), countAtTap)
      << "the reset has to emit a beat, or nothing on screen can update";
  EXPECT_EQ (lastBeat.load (), 0)
      << "beat 0 is what the status bar shows as 1/4";

  tempoClock.stop ();
}


// A clip fired from a pad lands on the next beat, not the next bar: a bar is
// up to four beats away and that is long enough to feel like the button did
// not work. The bar is still where a *take* is quantised — this is only about
// when a press takes effect.
TEST (TempoClock, TheNextBeatIsTheNextOneNotTheNextBar)
{
  constexpr int beatsPerBar = 4;

  // Mid-beat: on to the beat that follows.
  EXPECT_EQ (TempoClock::nextBeat ({ 3, 1, 40 }, beatsPerBar),
             Measure (3, 2, 0));

  // The last beat of a bar rolls into the next bar's first.
  EXPECT_EQ (TempoClock::nextBeat ({ 3, 3, 40 }, beatsPerBar),
             Measure (4, 0, 0));
}

// Already exactly on a beat is already the answer — the same rule
// nextDownBeat() follows for a bar. Rounding up here would put every press
// that landed on time a whole beat late.
TEST (TempoClock, AMeasureAlreadyOnABeatIsItsOwnNextBeat)
{
  EXPECT_EQ (TempoClock::nextBeat ({ 2, 0, 0 }, 4), Measure (2, 0, 0));
  EXPECT_EQ (TempoClock::nextBeat ({ 2, 3, 0 }, 4), Measure (2, 3, 0));
}

// Odd metres are a setting here, so the wrap cannot be hard-coded to four.
TEST (TempoClock, TheWrapFollowsTheMetre)
{
  EXPECT_EQ (TempoClock::nextBeat ({ 1, 2, 5 }, 3), Measure (2, 0, 0));
  EXPECT_EQ (TempoClock::nextBeat ({ 1, 4, 5 }, 7), Measure (1, 5, 0));
}
