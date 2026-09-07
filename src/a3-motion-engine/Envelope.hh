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

#pragma once

#include <a3-motion-engine/Playhead.hh>

#include <JuceHeader.h>

namespace a3
{

/** The clip's accent: an envelope you *play* rather than set.
 *
 *  ACT presses and it rises; ACT stays down and it stays up; ACT comes up and
 *  it falls. The hold is the finger, which is why there is no sustain control
 *  — on a pad, how long a thing lasts is a gesture, and a gesture beats a
 *  number you would have to have set beforehand for a moment you did not know
 *  was coming.
 *
 *  Its times run in bars off the tempo clock, like everything else here (see
 *  TempoLfo), because an accent that lands off the beat is not an accent.
 */

/** Attack and decay run 1/16 of a bar to 4 bars, in powers of two — a shorter
 *  reach than TempoLfo's, since this is a gesture rather than a cycle and
 *  nobody wants a thirty-two bar attack. */
constexpr int envelopeMaxStep = 6;

/** How many bars a stage takes, at step 0..envelopeMaxStep: 1/16, 1/8, 1/4,
 *  1/2, 1, 2, 4. */
float envelopeBarsForStep (int step);

enum class EnvelopeStage
{
  Idle,
  Attack,
  Hold,
  Decay,
};

struct EnvelopeState
{
  EnvelopeStage stage = EnvelopeStage::Idle;
  /** Where the envelope stands, 0 at rest and 1 at the top. */
  float level = 0.f;
};

/** Whether the envelope should stay at the top, given the mode and the finger.
 *
 *  One-shot fires and falls, however long the pad is held: the gesture is the
 *  press, not the press's length. Hold is the finger, which is what the two
 *  words have always meant -- and what the engine did not read, so a one-shot
 *  sustained like a hold with another name on it.
 */
bool envelopeHolds (ActMode mode, bool fingerDown);

/** ACT went down: the envelope starts rising, in either mode.
 *
 *  Separate from advanceEnvelope() because a press is an edge and running is
 *  a level, and conflating the two is what kept a one-shot silent: asking
 *  only "is it held" gets "no" for a one-shot even with a finger on the pad,
 *  so nothing ever left Idle.
 *
 *  The level is left where it is, so a second press during the fall turns the
 *  envelope round from there rather than dropping it to nothing first.
 */
EnvelopeState fireEnvelope (EnvelopeState state);

/** One tick on, given the mode and whether the pad is still down.
 *
 *  The finger only ever *ends* things here; starting is fireEnvelope()'s job.
 *  And it ends only a hold: a one-shot's attack runs to the top whatever the
 *  finger does, because an attack that a quick release could cut short would
 *  be a hold that nobody had named. */
EnvelopeState advanceEnvelope (EnvelopeState state, ActMode mode,
                               bool fingerDown, int attackStep, int decayStep,
                               float ticksPerBar);

/** What goes out: the value that was set, raised towards `maxValue` by the
 *  envelope.
 *
 *  The set value is the floor and stays it — the hardware pot and the grid
 *  keep meaning exactly what they meant, and what they mean now is the bottom
 *  of the swing rather than the whole of it. `maxValue` is the ceiling, which
 *  the clip carries: how far the accent throws is a thing you set rather than
 *  one you discover, and without it every accent went all the way to 1.
 *
 *  A ceiling under the floor leaves the floor alone. It is a setting somebody
 *  will make by accident, and an accent that pushed the value *down* would be
 *  a surprise in the one direction nothing else here moves. */
float envelopeOver (float setValue, float maxValue, float level);

}
