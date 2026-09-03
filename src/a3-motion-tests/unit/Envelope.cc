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

#include <a3-motion-engine/Envelope.hh>

using namespace a3;

namespace
{
constexpr float ticksPerBar = 128.f * 4.f; // ticksPerBeat x a four-beat bar

/** Run the envelope for a while and hand back where it ended up. */
EnvelopeState
run (EnvelopeState state, bool held, int ticks, int attack = 2, int decay = 2)
{
  for (int i = 0; i < ticks; ++i)
    state = advanceEnvelope (state, held, attack, decay, ticksPerBar);

  return state;
}
}

// The table, written out. A gesture's worth of lengths, not a cycle's.
TEST (Envelope, TheStagesRunFromASixteenthOfABarToFour)
{
  float const expected[]
      = { 1.f / 16.f, 1.f / 8.f, 1.f / 4.f, 1.f / 2.f, 1.f, 2.f, 4.f };

  for (int step = 0; step <= envelopeMaxStep; ++step)
    EXPECT_FLOAT_EQ (envelopeBarsForStep (step), expected[step])
        << "step " << step;

  // Past either end is the end.
  EXPECT_FLOAT_EQ (envelopeBarsForStep (-3), envelopeBarsForStep (0));
  EXPECT_FLOAT_EQ (envelopeBarsForStep (envelopeMaxStep + 3),
                   envelopeBarsForStep (envelopeMaxStep));
}

TEST (Envelope, AtRestItIsAtRest)
{
  auto const state = run ({}, false, 500);

  EXPECT_EQ (state.stage, EnvelopeStage::Idle);
  EXPECT_FLOAT_EQ (state.level, 0.f);
}

// Down: up in the attack's own time, and then it stays up. The staying is the
// point — it is what makes the length a thing you play.
TEST (Envelope, HeldItRisesInTheAttackTimeAndThenHolds)
{
  // Step 2 is a quarter bar.
  auto const attackTicks
      = static_cast<int> (envelopeBarsForStep (2) * ticksPerBar);

  auto state = run ({}, true, attackTicks / 2, 2, 2);
  EXPECT_EQ (state.stage, EnvelopeStage::Attack);
  EXPECT_NEAR (state.level, 0.5f, 0.02f);

  state = run (state, true, attackTicks, 2, 2);
  EXPECT_EQ (state.stage, EnvelopeStage::Hold);
  EXPECT_FLOAT_EQ (state.level, 1.f);

  // ... and it stays there, however long the finger stays.
  state = run (state, true, 2000, 2, 2);
  EXPECT_EQ (state.stage, EnvelopeStage::Hold);
  EXPECT_FLOAT_EQ (state.level, 1.f);
}

TEST (Envelope, LetGoItFallsInTheDecayTimeAndStops)
{
  auto const ticks = static_cast<int> (envelopeBarsForStep (2) * ticksPerBar);

  auto state = run ({}, true, ticks * 2, 2, 2);
  ASSERT_EQ (state.stage, EnvelopeStage::Hold);

  state = run (state, false, ticks / 2, 2, 2);
  EXPECT_EQ (state.stage, EnvelopeStage::Decay);
  EXPECT_NEAR (state.level, 0.5f, 0.02f);

  state = run (state, false, ticks, 2, 2);
  EXPECT_EQ (state.stage, EnvelopeStage::Idle);
  EXPECT_FLOAT_EQ (state.level, 0.f);
}

// Let go before it got to the top and it falls from where it is, not from the
// top. A jump to full on release would be the accent firing itself.
TEST (Envelope, LetGoMidAttackItFallsFromWhereItGot)
{
  auto const ticks = static_cast<int> (envelopeBarsForStep (2) * ticksPerBar);

  auto state = run ({}, true, ticks / 4, 2, 2);
  auto const reached = state.level;
  ASSERT_LT (reached, 0.5f);
  ASSERT_GT (reached, 0.f);

  state = advanceEnvelope (state, false, 2, 2, ticksPerBar);
  EXPECT_EQ (state.stage, EnvelopeStage::Decay);
  EXPECT_LE (state.level, reached);
}

// Pressed again while it is still falling, it picks up from where it is
// instead of restarting at nothing — two accents close together should not
// punch a hole between them.
TEST (Envelope, PressedAgainWhileFallingItRisesFromWhereItIs)
{
  auto const ticks = static_cast<int> (envelopeBarsForStep (2) * ticksPerBar);

  auto state = run ({}, true, ticks * 2, 2, 2);
  state = run (state, false, ticks / 2, 2, 2);
  auto const caught = state.level;
  ASSERT_GT (caught, 0.1f);

  state = advanceEnvelope (state, true, 2, 2, ticksPerBar);
  EXPECT_EQ (state.stage, EnvelopeStage::Attack);
  EXPECT_GE (state.level, caught);
}

// The one that matters for what comes out of the machine. Whatever the
// envelope is doing, the value stays between the floor the hand set and the
// ceiling the clip carries.
TEST (Envelope, TheSetValueIsAFloorAndMaxIsTheCeiling)
{
  for (float set : { 0.f, 0.25f, 0.5f, 0.9f, 1.f })
    for (float max : { 0.f, 0.4f, 0.8f, 1.f })
      for (int i = 0; i <= 20; ++i)
        {
          auto const level = static_cast<float> (i) / 20.f;
          auto const out = envelopeOver (set, max, level);

          EXPECT_GE (out, set)
              << "set " << set << " max " << max << " level " << level;
          EXPECT_LE (out, 1.f)
              << "set " << set << " max " << max << " level " << level;
        }

  // At rest it is exactly what was set — not nearly, exactly, or the pot
  // would drift every time the accent finished.
  for (float set : { 0.f, 0.3f, 1.f })
    EXPECT_EQ (envelopeOver (set, 1.f, 0.f), set);

  // At the top it is the ceiling, which is the point of having one: how far
  // the accent throws is a thing you set, not a thing you discover.
  for (float set : { 0.f, 0.3f })
    for (float max : { 0.5f, 0.75f, 1.f })
      EXPECT_FLOAT_EQ (envelopeOver (set, max, 1.f), max);
}

// A ceiling under the floor is a setting somebody will make by accident, and
// it must not send the value *down* — the floor is the floor.
TEST (Envelope, ACeilingBelowTheFloorStillLeavesTheFloorAlone)
{
  for (int i = 0; i <= 20; ++i)
    {
      auto const level = static_cast<float> (i) / 20.f;
      auto const out = envelopeOver (0.8f, 0.3f, level);

      EXPECT_FLOAT_EQ (out, 0.8f) << "level " << level;
    }
}
