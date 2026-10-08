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

#include <a3-motion-ui/components/ClockModeTempo.hh>

using namespace a3;

namespace
{
constexpr int ext = 1;
constexpr int pio = 2;
}

// Leaving INT keeps the tempo INT had, so coming back finds it.
TEST (ClockModeTempo, LeavingInternalKeepsItsTempo)
{
  EXPECT_FLOAT_EQ (internalTempoKept (clockModeInternal, ext, 0.f, 128.f),
                   128.f);
}

// #51: tapped to 128, round the modes, tapped to 140, round again -- INT came
// back at 128, because only the first leaving was kept.
TEST (ClockModeTempo, LeavingInternalAgainKeepsTheNewTempo)
{
  auto kept = internalTempoKept (clockModeInternal, ext, 0.f, 128.f);
  kept = internalTempoKept (ext, pio, kept, 0.f);
  kept = internalTempoKept (pio, clockModeInternal, kept, 0.f);

  kept = internalTempoKept (clockModeInternal, ext, kept, 140.f);
  EXPECT_FLOAT_EQ (kept, 140.f);
}

// EXT to PIO is not leaving INT: the engine runs on the outside clock then,
// and its tempo is not INT's.
TEST (ClockModeTempo, StepsBetweenOutsideClocksKeepTheInternalTempo)
{
  EXPECT_FLOAT_EQ (internalTempoKept (ext, pio, 128.f, 174.f), 128.f);
}

// Coming back to INT changes nothing kept; it is what gets restored.
TEST (ClockModeTempo, ArrivingAtInternalKeepsWhatWasKept)
{
  EXPECT_FLOAT_EQ (internalTempoKept (pio, clockModeInternal, 128.f, 0.f),
                   128.f);
}
