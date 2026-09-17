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


// In EXT and PIO the clock takes its phase from the beats it is sent, not only
// their tempo. Told mid-way through beat 0 that beat 2 is happening now, it has
// to be on beat 2 straight away -- not a second later, and not still on beat 0
// with the right tempo, which is what it used to do.
TEST (TempoClock, ABeatFromOutsideMovesTheClockOntoIt)
{
  TempoClock tempoClock;
  tempoClock.setTempoBPM (60.f);
  tempoClock.start ();

  std::atomic<int> lastBeat{ -1 };
  auto handle = tempoClock.scheduleEventHandlerAddition (
      [&lastBeat] (Measure measure) {
        lastBeat = static_cast<int> (measure.beat ());
      },
      TempoClock::Event::Beat, TempoClock::Execution::TimerThread, true);

  tempoClock.reset ();
  for (int i = 0; i < 100 && lastBeat.load () != 0; ++i)
    juce::Thread::sleep (2);
  ASSERT_EQ (lastBeat.load (), 0);

  juce::Thread::sleep (100);
  tempoClock.syncToBeat (2, TempoClock::monotonicNanoseconds ());

  for (int i = 0; i < 50 && lastBeat.load () != 2; ++i)
    juce::Thread::sleep (2);

  EXPECT_EQ (lastBeat.load (), 2)
      << "a beat from outside has to move the clock's phase, not only its "
         "tempo";

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

// The phase lock is worth nothing if nothing hands it a beat: a function with
// no caller is how a coupling goes missing in this project with every test
// green.
TEST (TempoClock, BeatsFromOutsideActuallyReachTheClock)
{
  juce::File const root (A3_UI_SOURCE_DIR);
  auto callers = 0;
  for (auto const &entry : juce::RangedDirectoryIterator (
           root, true, "*.cc", juce::File::findFiles))
    if (entry.getFile ().loadFileAsString ().contains ("syncToBeat ("))
      ++callers;

  EXPECT_GT (callers, 0) << "no /beat ever moves the clock's phase";
}
