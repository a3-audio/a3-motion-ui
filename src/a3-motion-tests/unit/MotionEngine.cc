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

}
