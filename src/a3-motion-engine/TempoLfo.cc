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

#include "TempoLfo.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** Step 1 is one cycle every 32 bars, and every step from there halves it.
 *  Written as an exponent so the table is one line instead of eight. */
constexpr int slowestExponent = 5; // 2^5 bars at step 1

int
clampedStep (int step)
{
  return std::clamp (step, -lfoMaxStep, lfoMaxStep);
}
}

float
lfoBarsPerCycle (int step)
{
  auto const magnitude = std::abs (clampedStep (step));
  if (magnitude == 0)
    return 0.f;

  return std::pow (2.f, static_cast<float> (slowestExponent - (magnitude - 1)));
}

float
lfoCyclesPerBar (int step)
{
  auto const bars = lfoBarsPerCycle (step);
  if (bars == 0.f)
    return 0.f;

  return (clampedStep (step) > 0 ? 1.f : -1.f) / bars;
}

float
advanceLfoPhase (float phase, int step, float ticksPerBar)
{
  auto const rate = lfoCyclesPerBar (step);
  if (rate == 0.f || ticksPerBar <= 0.f)
    return phase;

  auto const advanced = phase + rate / ticksPerBar;

  // std::fmod keeps the sign of its left operand, so a cycle running the other
  // way would come back negative and every reader would have to know that.
  // Wrapped into [0, 1) here, once.
  auto wrapped = std::fmod (advanced, 1.f);
  if (wrapped < 0.f)
    wrapped += 1.f;

  return wrapped;
}

float
lfoTravel (float phase)
{
  return 0.5f - 0.5f * std::cos (phase * juce::MathConstants<float>::twoPi);
}

float
lfoSweep (float from, int step, float phase)
{
  if (clampedStep (step) == 0)
    return from;

  // The sign says which end, and the value says how far it is to it. A sweep
  // that ran symmetrically either side of `from` would shrink to nothing as
  // `from` approached a limit, which is a control quietly switching itself off.
  auto const to = clampedStep (step) > 0 ? 1.f : 0.f;

  return from + (to - from) * lfoTravel (phase);
}

}
