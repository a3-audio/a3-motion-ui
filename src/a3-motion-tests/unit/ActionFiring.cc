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

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/MotionEngine.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/elevation/HeightMapSphere.hh>

#include <algorithm>
#include <string>
#include <vector>

#include "ClipSettingsFields.hh"

using namespace a3;

namespace
{
/** The fields ACT plays rather than aims: the two envelopes' times and
 *  ceilings and whether ACT is a stab or a hold. They belong to the slot
 *  because that is what the ACTION page shows and edits. */
std::vector<std::string> const slotOwned{
  "envelopeAttack", "envelopeDecay", "envelopeMax", "freqAttack",
  "freqDecay",      "freqMax",       "qAttack",     "qDecay",
  "qMax",           "actMode",
};

bool
isSlotOwned (char const *field)
{
  return std::find (slotOwned.begin (), slotOwned.end (), std::string (field))
         != slotOwned.end ();
}
}

// ── What a fired action reaches ──────────────────────────────────────────

// An action says *where* the clip is thrown; the accent says *how*. Splitting
// them there is what keeps the ACTION page honest: every number on it stays
// the slot's, so it cannot show one attack while a fired action runs another.
TEST (ActionFiring, AnActionDrivesWhereTheClipGoes)
{
  ClipSettings current;

  ClipSettings action;
  action.speedLog2 = -2;
  action.reach = 0.9f;
  action.spin = 4;
  action.direction = PlayDirection::Reverse;
  action.endAction = EndAction::Bounce;
  action.fadeReach = 0.8f;
  action.bridgeBias = -3;

  auto const fired = actionOver (current, action);

  EXPECT_EQ (fired.speedLog2, -2);
  EXPECT_FLOAT_EQ (fired.reach, 0.9f);
  EXPECT_EQ (fired.spin, 4);
  EXPECT_EQ (fired.direction, PlayDirection::Reverse);
  EXPECT_EQ (fired.endAction, EndAction::Bounce);
  EXPECT_FLOAT_EQ (fired.fadeReach, 0.8f);
  EXPECT_EQ (fired.bridgeBias, -3);
}

TEST (ActionFiring, HowTheAccentIsPlayedStaysWithTheSlot)
{
  ClipSettings current;
  current.envelopeAttack = 0;
  current.envelopeDecay = 5;
  current.envelopeMax = 0.4f;
  current.freqAttack = 1;
  current.freqDecay = 6;
  current.freqMax = 0.7f;
  current.qAttack = 5;
  current.qDecay = 1;
  current.qMax = 0.3f;
  current.actMode = ActMode::Hold;

  ClipSettings action;
  action.envelopeAttack = 6;
  action.envelopeDecay = 0;
  action.envelopeMax = 1.f;
  action.freqAttack = 6;
  action.freqDecay = 0;
  action.freqMax = 0.f;
  action.qAttack = 0;
  action.qDecay = 6;
  action.qMax = 1.f;
  action.actMode = ActMode::OneShot;

  auto const fired = actionOver (current, action);

  EXPECT_EQ (fired.envelopeAttack, 0);
  EXPECT_EQ (fired.envelopeDecay, 5);
  EXPECT_FLOAT_EQ (fired.envelopeMax, 0.4f);
  EXPECT_EQ (fired.freqAttack, 1);
  EXPECT_EQ (fired.freqDecay, 6);
  EXPECT_FLOAT_EQ (fired.freqMax, 0.7f);
  EXPECT_EQ (fired.qAttack, 5);
  EXPECT_EQ (fired.qDecay, 1);
  EXPECT_FLOAT_EQ (fired.qMax, 0.3f);
  EXPECT_EQ (fired.actMode, ActMode::Hold)
      << "the mode was read when ACT went down; changing it mid-gesture "
         "leaves a held clip with nothing holding it";
}

// A field added to ClipSettings and forgotten here would quietly join the
// driven half, because actionOver copies the action wholesale. This walks the
// shared list so that adding a field forces the decision rather than making
// it by default.
TEST (ActionFiring, EveryFieldIsEitherDrivenOrLeftWithTheSlot)
{
  for (auto const &[name, mutate] : clipSettingsFields ())
    {
      ClipSettings action;
      mutate (action);

      auto const fired = actionOver (ClipSettings{}, action);

      if (isSlotOwned (name))
        EXPECT_EQ (fired, ClipSettings{})
            << name << " is the slot's, so a fired action must not move it";
      else
        EXPECT_NE (fired, ClipSettings{})
            << name << " is not driven by a fired action -- if that is right, "
                       "say so in slotOwned; if not, actionOver drops it";
    }
}

// ── Firing and falling back on a pattern ─────────────────────────────────

TEST (ActionFiring, FallingBackPutsEveryFieldWhereItWas)
{
  Pattern pattern;
  pattern.setSpin (2);
  pattern.setReach (0.3f);
  pattern.setEndAction (EndAction::Pause);
  pattern.setEnvelopeAttack (1);

  auto const before = clipSettingsFrom (pattern);

  ClipSettings action;
  action.spin = -5;
  action.reach = 1.f;
  action.endAction = EndAction::Bounce;

  applyClipSettings (pattern, actionOver (before, action));
  EXPECT_NE (clipSettingsFrom (pattern), before) << "the action did nothing";
  EXPECT_EQ (pattern.getEnvelopeAttack (), 1)
      << "the accent's shape is not the action's to change";

  applyClipSettings (pattern, before);
  EXPECT_EQ (clipSettingsFrom (pattern), before);
}

// ── Through the engine, on the clock ─────────────────────────────────────

// The parts above are exact; this is the one that says they are wired to each
// other. It runs the real tempo clock, because the accent is advanced from
// the tick callback and the fall back is hung off the edge where its decay
// runs out -- neither of which a pure test can reach.
namespace
{
/** A clip with the shortest accent there is, so a whole gesture fits inside a
 *  test: at 240 BPM a bar is a second, and a sixteenth of one is 62 ms. */
std::shared_ptr<Pattern>
clipWithAShortAccent (ActMode mode)
{
  auto pattern = std::make_shared<Pattern> ();
  pattern->setEnvelopeAttack (0);
  pattern->setEnvelopeDecay (0);
  pattern->setActMode (mode);
  pattern->setSpin (0);

  return pattern;
}
}

TEST (ActionFiring, AOneShotThrowsTheClipAndItComesBack)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true); // nothing leaves the machine from a test
  engine.setTempoBPM (240.f);

  auto pattern = clipWithAShortAccent (ActMode::OneShot);
  auto const before = clipSettingsFrom (*pattern);

  ClipSettings action;
  action.spin = 5;
  action.endAction = EndAction::Bounce;

  engine.setChannelAction (0, action);
  engine.setChannelAccentHeld (0, true, pattern);

  EXPECT_EQ (pattern->getSpin (), 5) << "the action never reached the clip";
  ASSERT_NE (clipSettingsFrom (*pattern), before);

  // The finger stays down the whole time: a one-shot is over when its
  // envelope is, not when the hand moves.
  juce::Thread::sleep (600);

  EXPECT_EQ (clipSettingsFrom (*pattern), before)
      << "the clip never came back from the action it was thrown to";
}

TEST (ActionFiring, AHoldComesBackWhenTheFingerLetsGo)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto pattern = clipWithAShortAccent (ActMode::Hold);
  auto const before = clipSettingsFrom (*pattern);

  ClipSettings action;
  action.spin = -4;

  engine.setChannelAction (0, action);
  engine.setChannelAccentHeld (0, true, pattern);
  ASSERT_EQ (pattern->getSpin (), -4);

  // Still down, still the action's: a hold lasts exactly as long as the
  // finger, and nothing about the clock may end it early.
  juce::Thread::sleep (400);
  EXPECT_EQ (pattern->getSpin (), -4)
      << "a hold gave the clip back while the pad was still down";

  engine.setChannelAccentHeld (0, false, nullptr);
  juce::Thread::sleep (400);

  EXPECT_EQ (clipSettingsFrom (*pattern), before);
}

// A slot with nothing assigned fires the accent and leaves the clip alone --
// which is every slot until somebody saves an action, so it is the case that
// has to stay silent.
TEST (ActionFiring, WithNoActionOnTheSlotTheClipIsNotTouched)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto pattern = clipWithAShortAccent (ActMode::OneShot);
  auto const before = clipSettingsFrom (*pattern);

  engine.setChannelAction (0, std::nullopt);
  engine.setChannelAccentHeld (0, true, pattern);

  EXPECT_EQ (clipSettingsFrom (*pattern), before);

  juce::Thread::sleep (400);
  EXPECT_EQ (clipSettingsFrom (*pattern), before);
}

// The end action is one of the things an action carries, and the only moment
// it can act is the one where the accent runs out. Falling back before it is
// read hands that moment to the clip's own setting instead -- an action that
// says "stop" would then have said nothing at all.
TEST (ActionFiring, AnActionsEndActionIsWhatEndsTheAccent)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto pattern = clipWithAShortAccent (ActMode::OneShot);
  pattern->setEndAction (EndAction::Loop);
  pattern->setPlaybackLength (Measure{ 1, 0, 0 });
  engine.playPattern (pattern, Measure{});

  ClipSettings action;
  action.endAction = EndAction::Stop;

  engine.setChannelAction (0, action);
  engine.setChannelAccentHeld (0, true, pattern);

  juce::Thread::sleep (600);

  EXPECT_EQ (pattern->getStatus (), Pattern::Status::Idle)
      << "the action said stop and the clip kept going";
  EXPECT_EQ (pattern->getEndAction (), EndAction::Loop)
      << "and afterwards the clip is its looping self again";
}

// One envelope for both was the first guess and it does not survive being
// heard: freq and Q are two gestures, not one, and a sweep that has to reach
// its resonance at exactly the speed it reaches its cutoff is a sweep with one
// shape. Two sets of three, and this is what says they are actually separate.
TEST (ActionFiring, FreqAndQSweepOnEnvelopesOfTheirOwn)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto pattern = clipWithAShortAccent (ActMode::Hold);
  pattern->setFreqAttack (envelopeMaxStep); // four bars: barely moving
  pattern->setQAttack (0);                  // a sixteenth: there at once
  pattern->setFreqMax (1.f);
  pattern->setQMax (1.f);

  // Both floors at nothing, so what is read back is the envelope alone.
  engine.setChannelPot1 (0, 0.f);
  engine.setChannelPot2 (0, 0.f);

  engine.setChannelAccentHeld (0, true, pattern);
  juce::Thread::sleep (300);

  EXPECT_GT (engine.getChannelPot2Effective (0),
             engine.getChannelPot1Effective (0))
      << "q could not outrun freq -- they are still riding one envelope";

  engine.setChannelAccentHeld (0, false, nullptr);
}
