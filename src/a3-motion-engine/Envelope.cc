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

bool
envelopeHolds (ActMode mode, bool fingerDown)
{
  return mode == ActMode::Hold && fingerDown;
}

EnvelopeState
fireEnvelope (EnvelopeState state)
{
  // From wherever it stands, not from nothing: two accents close together
  // should not punch a hole between them.
  state.stage = EnvelopeStage::Attack;

  return state;
}

EnvelopeState
advanceEnvelope (EnvelopeState state, ActMode mode, bool fingerDown,
                 int attackStep, int decayStep, float ticksPerBar)
{
  if (ticksPerBar <= 0.f)
    return state;

  // A hold belongs to the finger and turns round the moment it lifts, from
  // wherever it got to. A one-shot does not: the press is the whole gesture,
  // so its attack finishes whatever the hand does afterwards.
  if (mode == ActMode::Hold && !fingerDown
      && (state.stage == EnvelopeStage::Attack
          || state.stage == EnvelopeStage::Hold))
    state.stage = EnvelopeStage::Decay;

  switch (state.stage)
    {
    case EnvelopeStage::Attack:
      state.level += stepPerTick (attackStep, ticksPerBar);
      if (state.level >= 1.f)
        {
          state.level = 1.f;
          // The top is where the two modes part company: one stays there for
          // as long as the finger does, the other turns straight round.
          state.stage = envelopeHolds (mode, fingerDown) ? EnvelopeStage::Hold
                                                         : EnvelopeStage::Decay;
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
