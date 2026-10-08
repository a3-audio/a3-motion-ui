/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/flight/BeatPulse.hh>
#include <a3-motion-engine/tempo/TempoClock.hh>

using namespace a3;

namespace
{

constexpr int fourFour = 4;
constexpr int ticksPerBeat = TempoClock::getTicksPerBeat ();

float
pulseAt (int beat, int tick, FlightTuning const &tuning = {})
{
  return gravityPulse (Measure (0, beat, tick), fourFour, tuning);
}

}

TEST (BeatPulse, TheOnsetPullsHarderThanMidBeat)
{
  EXPECT_GT (pulseAt (1, 0), pulseAt (1, ticksPerBeat / 2));
}

TEST (BeatPulse, TheOneOfTheBarPullsHardest)
{
  EXPECT_GT (pulseAt (0, 0), pulseAt (2, 0));
}

TEST (BeatPulse, ItRelaxesToOneBeforeTheNextBeat)
{
  EXPECT_NEAR (pulseAt (0, ticksPerBeat - 1), 1.f, 1e-3f);
  EXPECT_NEAR (pulseAt (3, ticksPerBeat - 1), 1.f, 1e-3f);
}

TEST (BeatPulse, DepthZeroIsConstant)
{
  FlightTuning flat;
  flat.pulseDepth = 0.f;
  flat.pulseDownbeatDepth = 0.f;
  for (auto beat = 0; beat < fourFour; ++beat)
    for (auto tick = 0; tick < ticksPerBeat; ++tick)
      EXPECT_FLOAT_EQ (pulseAt (beat, tick, flat), 1.f)
          << "beat " << beat << " tick " << tick;
}

TEST (BeatPulse, NeverBelowOne)
{
  for (auto beat = 0; beat < fourFour; ++beat)
    for (auto tick = 0; tick < ticksPerBeat; ++tick)
      EXPECT_GE (pulseAt (beat, tick), 1.f)
          << "beat " << beat << " tick " << tick;
}
