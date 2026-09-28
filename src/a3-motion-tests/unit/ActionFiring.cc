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

#include "WaitUntil.hh"

#include <atomic>

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-engine/ActionScript.hh>
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
/** The fields a fired action leaves with the clip. None since 2026-09-27:
 *  how ACT is played -- the envelopes and the mode -- came to each of the six
 *  buttons (ActionFeel), which the action carries when it is fired. */
std::vector<std::string> const slotOwned{};

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
  action.endAction = EndAction::Pause;
  action.fadeReach = 0.8f;
  action.bridgeBias = -3;

  auto const fired = actionOver (current, action);

  EXPECT_EQ (fired.speedLog2, -2);
  EXPECT_FLOAT_EQ (fired.reach, 0.9f);
  EXPECT_EQ (fired.spin, 4);
  EXPECT_EQ (fired.direction, PlayDirection::Reverse);
  EXPECT_EQ (fired.endAction, EndAction::Pause);
  EXPECT_FLOAT_EQ (fired.fadeReach, 0.8f);
  EXPECT_EQ (fired.bridgeBias, -3);
}

// How the accent is played comes with the button that fired it (2026-09-27):
// six buttons on one clip are six feels, so the envelopes and the mode travel
// in the action rather than staying the clip's. The mode is still read when
// ACT goes down, from the button, so it cannot change mid-gesture.
TEST (ActionFiring, HowTheAccentIsPlayedComesWithTheButton)
{
  ClipSettings current;
  current.envelopeAttack = 0;
  current.envelopeDecay = 5;
  current.actMode = ActMode::Hold;

  ActionFeel feel;
  feel.envelopeAttack = 6;
  feel.envelopeDecay = 0;
  feel.qMax = 1.f;
  feel.actMode = ActMode::OneShot;
  auto const action = withFeel (ClipSettings{}, feel);

  auto const fired = actionOver (current, action);

  EXPECT_EQ (fired.envelopeAttack, 6);
  EXPECT_EQ (fired.envelopeDecay, 0);
  EXPECT_FLOAT_EQ (fired.qMax, 1.f);
  EXPECT_EQ (fired.actMode, ActMode::OneShot);
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
            << name << " is the clip's, so a fired action must not move it";
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
  action.endAction = EndAction::Stop;

  applyClipSettings (pattern, actionOver (before, action));
  EXPECT_NE (clipSettingsFrom (pattern), before) << "the action did nothing";
  EXPECT_EQ (pattern.getEnvelopeAttack (), action.envelopeAttack)
      << "the accent is played the way the button says (2026-09-27)";

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
  // A bounce keeps playing through the accent, as the bounce end action did.
  action.direction = PlayDirection::Bounce;

  engine.setChannelAction (0, action);
  engine.setChannelAccentHeld (0, true, pattern);

  // Awaited rather than read straight away: since 2026-09-21 the press is a
  // queued command, drained at the top of the next tick, because the accent
  // state belongs to the tempo-clock thread. Four milliseconds at 120 BPM.
  EXPECT_TRUE (waitUntil ([&] { return pattern->getSpin () == 5; }))
      << "the action never reached the clip";
  ASSERT_NE (clipSettingsFrom (*pattern), before);

  // Wie viele Ticks waehrend des Wartens kamen. Das ist die Zahl, die den
  // Befund vom 2026-09-21 entschieden hat: 2048 in vier Sekunden, also genau
  // die erwarteten sechzehn Beats bei 240 BPM. Die Clock lief einwandfrei --
  // womit "der Rechner war zu langsam" ausgeschieden war und nur noch der
  // Zustand uebrig blieb. Bleibt stehen, weil die naechste Untersuchung
  // dieselbe Frage zuerst stellen wird.
  std::atomic<int> ticks{ 0 };
  auto handle = engine.getTempoClock ().scheduleEventHandlerAddition (
      [&ticks] (Measure) { ++ticks; },
      TempoClock::Event::Tick, TempoClock::Execution::TimerThread);

  // The finger stays down the whole time: a one-shot is over when its
  // envelope is, not when the hand moves.
  EXPECT_TRUE (waitUntil ([&] { return clipSettingsFrom (*pattern) == before; }))
      << "the engine never got there -- ticks seen while waiting: "
      << ticks.load ();

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
  ASSERT_TRUE (waitUntil ([&] { return pattern->getSpin () == -4; }))
      << "the action never reached the clip";

  // Still down, still the action's: a hold lasts exactly as long as the
  // finger, and nothing about the clock may end it early.
  //
  // A real sleep, and rightly so: there is no moment at which "still
  // unchanged" becomes true, so there is nothing to poll for. Waiting longer
  // only makes this a stronger statement, which is why this one never
  // flickered.
  juce::Thread::sleep (400);
  EXPECT_EQ (pattern->getSpin (), -4)
      << "a hold gave the clip back while the pad was still down";

  engine.setChannelAccentHeld (0, false, nullptr);
  EXPECT_TRUE (waitUntil ([&] { return clipSettingsFrom (*pattern) == before; }))
      << "the engine never got there";

  EXPECT_EQ (clipSettingsFrom (*pattern), before);
}

// Six action buttons per channel (2026-09-27): a second button pressed while
// the first one's accent is still up takes over -- last press wins -- and the
// clip still comes home to its own settings, not to the first action's.
// Before, the second press kept the first action on, since a clip already
// wearing an action was not given another.
TEST (ActionFiring, ASecondButtonDuringAnAccentTakesOver)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true);
  engine.setTempoBPM (240.f);

  auto pattern = clipWithAShortAccent (ActMode::Hold);
  auto const before = clipSettingsFrom (*pattern);

  ClipSettings first;
  first.spin = -4;
  engine.setChannelAction (0, first);
  engine.setChannelAccentHeld (0, true, pattern);
  ASSERT_TRUE (waitUntil ([&] { return pattern->getSpin () == -4; }))
      << "the first action never reached the clip";

  ClipSettings second;
  second.spin = 3;
  engine.setChannelAction (0, second);
  engine.setChannelAccentHeld (0, true, pattern);
  EXPECT_TRUE (waitUntil ([&] { return pattern->getSpin () == 3; }))
      << "the second button did not take over";

  engine.setChannelAccentHeld (0, false, nullptr);
  EXPECT_TRUE (waitUntil ([&] { return clipSettingsFrom (*pattern) == before; }))
      << "the clip came back to the first action, not to itself";
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

  // Nothing to wait *for* -- the point is that nothing happens. But the press
  // is queued now, so the tick has to have run at least once, or this would
  // only be proving that the queue is still full.
  EXPECT_TRUE (waitUntil ([&] { return engine.isChannelAccentActive (0); }))
      << "the press never reached the clock";

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

  EXPECT_TRUE (waitUntil ([&] { return pattern->getStatus () == Pattern::Status::Idle; }))
      << "the engine never got there";

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
  EXPECT_TRUE (waitUntil ([&] {
    return engine.getChannelPot2Effective (0)
           > engine.getChannelPot1Effective (0);
  }))
      << "the engine never got there";

  EXPECT_GT (engine.getChannelPot2Effective (0),
             engine.getChannelPot1Effective (0))
      << "q could not outrun freq -- they are still riding one envelope";

  engine.setChannelAccentHeld (0, false, nullptr);
}

// How an action is played -- its three envelopes and its mode -- belongs to
// each of the six buttons from 2026-09-27, not to the clip. It is carried as
// an ActionFeel and put onto settings field by field.
TEST (ActionFiring, AFeelIsTheEnvelopesAndTheMode)
{
  ClipSettings clip;
  clip.envelopeAttack = 5;
  clip.qMax = 0.25f;
  clip.actMode = ActMode::Hold;
  clip.spin = 3;

  auto const feel = actionFeelFrom (clip);
  EXPECT_EQ (feel.envelopeAttack, 5);
  EXPECT_FLOAT_EQ (feel.qMax, 0.25f);
  EXPECT_EQ (feel.actMode, ActMode::Hold);

  auto const back = withFeel (ClipSettings{}, feel);
  EXPECT_EQ (back.envelopeAttack, 5);
  EXPECT_EQ (back.actMode, ActMode::Hold);
  EXPECT_EQ (back.spin, ClipSettings{}.spin) << "only the feel is put on";
}

TEST (ActionFiring, AFreshFeelIsTheClipDefaults)
{
  EXPECT_EQ (ActionFeel{}, actionFeelFrom (ClipSettings{}));
}


// -- Resolved at the press (2026-09-28) ---------------------------------------
//
// A button used to keep what its script made of the clip at the moment it
// was assigned -- every field of it. Load another clip, or turn a knob, and
// the next press threw the channel back to the old clip's values for as long
// as the accent ran. Now a script is worked out against the clip as it
// stands when the button goes down, with the dice the button rolled when it
// was assigned.

TEST (ActionFiring, AnActionLeavesWhatItDoesNotNameAsTheClipHasItNow)
{
  juce::String const script = "~reach = ~reach * 0.5;";
  ClipSettings first;
  first.rotate = 0.1f;
  first.reach = 0.8f;
  ClipSettings second = first;
  second.rotate = 0.7f;
  second.reach = 0.4f;

  auto const onFirst = resolveActionAt (script, first, 1, ActionFeel{});
  auto const onSecond = resolveActionAt (script, second, 1, ActionFeel{});

  EXPECT_FLOAT_EQ (onFirst.rotate, 0.1f);
  EXPECT_FLOAT_EQ (onFirst.reach, 0.4f);
  EXPECT_FLOAT_EQ (onSecond.rotate, 0.7f);
  EXPECT_FLOAT_EQ (onSecond.reach, 0.2f);
}

TEST (ActionFiring, TheSameSeedRollsTheSameDiceOnEveryPress)
{
  juce::String const script = "~rotate = rrand(0.0, 1.0);";
  ClipSettings const clip;
  EXPECT_FLOAT_EQ (resolveActionAt (script, clip, 42, ActionFeel{}).rotate,
                   resolveActionAt (script, clip, 42, ActionFeel{}).rotate);
}

TEST (ActionFiring, TheButtonsFeelIsOnTheResolvedAction)
{
  ActionFeel feel;
  feel.envelopeAttack = 5;
  feel.actMode = ActMode::Hold;
  auto const fired = resolveActionAt ("~reach = 0.3;", ClipSettings{}, 1, feel);
  EXPECT_EQ (fired.envelopeAttack, 5);
  EXPECT_EQ (fired.actMode, ActMode::Hold);
}

// The engine counts accents that have ended -- on the edge where the clip is
// given back -- so the message thread can decide what a button's "then"
// fires without any script being worked out on the clock's thread.
TEST (ActionFiring, TheEngineCountsEachAccentThatEnds)
{
  HeightMapSphere heightMap;
  MotionEngine engine (4, heightMap);
  engine.setPreviewMode (0, true); // nothing leaves the machine from a test
  engine.setTempoBPM (240.f);

  auto pattern = clipWithAShortAccent (ActMode::OneShot);
  ClipSettings action = clipSettingsFrom (*pattern);
  action.spin = 5;

  EXPECT_EQ (engine.accentEndCount (0), 0u);

  engine.setChannelAction (0, action);
  engine.setChannelAccentHeld (0, true, pattern);
  engine.setChannelAccentHeld (0, false, nullptr);

  EXPECT_TRUE (waitUntil ([&] { return engine.accentEndCount (0) == 1u; }))
      << "the end of the accent was never counted";
  EXPECT_TRUE (waitUntil ([&] { return !engine.isChannelAccentActive (0); }));
  EXPECT_EQ (engine.accentEndCount (0), 1u) << "counted more than once";
  EXPECT_EQ (engine.accentEndCount (1), 0u) << "another channel's count moved";
}
