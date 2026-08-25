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

#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

using namespace a3;

namespace
{

TEST (MotionEngine, TempoFacadeForwardsToTempoClock)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);

  // Defaults come straight from TempoClock's own defaults.
  EXPECT_EQ (engine.getBeatsPerBar (), 4);

  engine.setTempoBPM (140.f);
  EXPECT_FLOAT_EQ (engine.getTempoBPM (), 140.f);

  // First tap after construction always reports FirstTap (per
  // TempoClock::TapResult's own semantics — this just confirms the facade
  // reaches the real TempoClock instance, not a copy).
  auto const result = engine.tap (juce::Time::getHighResolutionTicks ());
  EXPECT_EQ (result, TempoClock::TapResult::FirstTap);

  // TempoClock::reset() only resets metrical position (bar/beat/tick),
  // never the tempo itself — this just confirms resetTempo() reaches the
  // real instance and doesn't accidentally clear BPM as a side effect.
  engine.resetTempo ();
  EXPECT_FLOAT_EQ (engine.getTempoBPM (), 140.f);
}


// The first tap is supposed to put the beat back to 1. TempoClock::tap() calls
// reset() for it, and the clock's timer thread applies that by zeroing its
// measure and emitting Tick/Beat/Bar. This walks that whole path, because
// reading it told us nothing — every link looked correct while the rig still
// showed the beat not resetting in INT mode.
TEST (MotionEngine, FirstTapPutsTheBeatBackToOne)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);

  std::atomic<int> lastBeat{ -1 };
  std::atomic<int> beatCallbacks{ 0 };

  auto handle = engine.getTempoClock ().scheduleEventHandlerAddition (
      [&lastBeat, &beatCallbacks] (Measure measure) {
        lastBeat = static_cast<int> (measure.beat ());
        ++beatCallbacks;
      },
      TempoClock::Event::Beat, TempoClock::Execution::TimerThread);

  // Let the clock run far enough into a bar that a reset is visible as a
  // change rather than as the state it was already in.
  engine.setTempoBPM (240.f);
  juce::Thread::sleep (600);
  ASSERT_GT (beatCallbacks.load (), 0) << "the clock is not ticking at all";

  auto const before = beatCallbacks.load ();
  engine.tap (juce::Time::getHighResolutionTicks ());
  juce::Thread::sleep (100);

  EXPECT_GT (beatCallbacks.load (), before)
      << "the tap produced no beat event, so reset() never reached the timer";
  EXPECT_EQ (lastBeat.load (), 0)
      << "the beat did not go back to the start of the bar";
}

}
