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

#include <a3-motion-engine/PlaybackRate.hh>

using namespace a3;

// Speed was the length: lengthBars = 2^speedLog2, and the pattern's own
// data-beats was ignored. Turning the knob therefore redefined how long a take
// had been, after the fact. It is a factor now.

TEST (PlaybackRate, ZeroPlaysThePatternAtItsOwnLength)
{
  EXPECT_FLOAT_EQ (playbackLengthBeats (16.f, 0), 16.f);
  EXPECT_FLOAT_EQ (playbackLengthBeats (4.f, 0), 4.f);
}

// The direction is the one the knob already had — a larger exponent is a
// longer traversal, which is slower. A control that reverses after an update
// is a nuisance on stage.
TEST (PlaybackRate, ALargerExponentIsSlower)
{
  EXPECT_FLOAT_EQ (playbackLengthBeats (4.f, 1), 8.f);
  EXPECT_FLOAT_EQ (playbackLengthBeats (4.f, 2), 16.f);
}

TEST (PlaybackRate, ANegativeExponentIsFaster)
{
  EXPECT_FLOAT_EQ (playbackLengthBeats (4.f, -1), 2.f);
  EXPECT_FLOAT_EQ (playbackLengthBeats (4.f, -2), 1.f);
}

// Two patterns of different length at the same rate keep their proportion —
// that is the whole point of separating the two numbers.
TEST (PlaybackRate, TheRateIsRelativeToWhateverThePatternIs)
{
  EXPECT_FLOAT_EQ (playbackLengthBeats (32.f, -1), 16.f);
  EXPECT_FLOAT_EQ (playbackLengthBeats (8.f, -1), 4.f);
}

// A traversal shorter than one beat has to be expressible, or the speed knob
// runs out of effect long before it runs out of travel. It used to be handed on
// as a whole number of beats, clamped up to one: on a four-beat pattern that is
// 2^-2, and the four detents below it did nothing at all.
TEST (PlaybackRate, AFasterRateThanOneBeatSurvives)
{
  auto constexpr ticksPerBeat = 128;

  EXPECT_EQ (playbackLengthTicks (4.f, ticksPerBeat), 512u);
  EXPECT_EQ (playbackLengthTicks (1.f, ticksPerBeat), 128u);
  EXPECT_EQ (playbackLengthTicks (0.5f, ticksPerBeat), 64u)
      << "half a beat was rounded up to a whole one";
  EXPECT_EQ (playbackLengthTicks (0.125f, ticksPerBeat), 16u);
}

// Every detent of the knob has to change something, all the way down.
TEST (PlaybackRate, EveryStepOfTheKnobChangesTheRate)
{
  auto constexpr ticksPerBeat = 128;
  auto const patternBeats = 4.f;

  index_t previous = 0;
  for (int speedLog2 = 0; speedLog2 >= -7; --speedLog2)
    {
      auto const ticks = playbackLengthTicks (
          playbackLengthBeats (patternBeats, speedLog2), ticksPerBeat);

      EXPECT_GT (ticks, 0u) << "speedLog2 " << speedLog2;
      if (previous > 0)
        EXPECT_LT (ticks, previous)
            << "speedLog2 " << speedLog2 << " is no faster than the step above";
      previous = ticks;
    }
}
