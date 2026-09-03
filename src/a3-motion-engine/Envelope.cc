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

#include "Envelope.hh"

#include <algorithm>
#include <cmath>

namespace a3
{

namespace
{
/** Step 0 is a sixteenth of a bar and every step doubles it. */
constexpr int shortestExponent = -4; // 2^-4 bars at step 0

/** How far the level moves in one tick of a stage of this many bars. A stage
 *  shorter than a tick moves the whole way at once rather than not at all. */
float
stepPerTick (int step, float ticksPerBar)
{
  auto const ticks = envelopeBarsForStep (step) * ticksPerBar;
  if (ticks <= 1.f)
    return 1.f;

  return 1.f / ticks;
}
}

float
envelopeBarsForStep (int step)
{
  auto const clamped = std::clamp (step, 0, envelopeMaxStep);

  return std::pow (2.f, static_cast<float> (shortestExponent + clamped));
}

EnvelopeState
advanceEnvelope (EnvelopeState state, bool held, int attackStep,
                 int decayStep, float ticksPerBar)
{
  if (ticksPerBar <= 0.f)
    return state;

  // The finger decides which way it is going, always — a press caught during
  // the fall turns it around from where it is rather than starting again at
  // nothing, so two accents close together do not punch a hole between them.
  if (held)
    {
      if (state.stage != EnvelopeStage::Hold)
        state.stage = EnvelopeStage::Attack;
    }
  else if (state.stage == EnvelopeStage::Attack
           || state.stage == EnvelopeStage::Hold)
    {
      state.stage = EnvelopeStage::Decay;
    }

  switch (state.stage)
    {
    case EnvelopeStage::Attack:
      state.level += stepPerTick (attackStep, ticksPerBar);
      if (state.level >= 1.f)
        {
          state.level = 1.f;
          state.stage = EnvelopeStage::Hold;
        }
      break;

    case EnvelopeStage::Hold:
      state.level = 1.f;
      break;

    case EnvelopeStage::Decay:
      state.level -= stepPerTick (decayStep, ticksPerBar);
      if (state.level <= 0.f)
        {
          state.level = 0.f;
          state.stage = EnvelopeStage::Idle;
        }
      break;

    case EnvelopeStage::Idle:
      state.level = 0.f;
      break;
    }

  return state;
}

float
envelopeOver (float setValue, float maxValue, float level)
{
  // Exactly the set value at rest rather than nearly it: this is read every
  // tick and sent when it changes, and a value that came back a hair off
  // would have the pot drifting every time an accent finished.
  if (level <= 0.f)
    return setValue;

  // A ceiling under the floor is a setting somebody will make by accident,
  // and an accent that pushed the value down would surprise in the one
  // direction nothing else here moves.
  if (maxValue <= setValue)
    return setValue;

  auto const clamped = std::clamp (level, 0.f, 1.f);

  return setValue + (maxValue - setValue) * clamped;
}

}
