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
#include <a3-motion-engine/Pattern.hh>

#include "ClipSettingsFields.hh"

#include <functional>
#include <utility>
#include <vector>

using namespace a3;

// Every value the clip settings menu shows has to survive the round trip
// through ClipSettings. A field that is read but never written is a setting
// that silently resets, which is the whole failure this change is about.
TEST (ClipSettings, EveryValueSurvivesLeavingAPatternAndComingBack)
{
  Pattern source;
  source.setSpeedLog2 (-1);
  source.setFadeReach (0.75f);
  source.setBridgeBias (-3);
  source.setRotate (0.25f);
  source.setReach (0.4f);
  source.setClipTop (0.1f);
  source.setClipBottom (0.2f);
  source.setMirrorSouth (true);
  source.setFlat (true);
  source.setFlatElevation (0.3f);
  source.setSpin (2);
  source.setReachLfo (-3);
  source.setEnvelopeAttack (1);
  source.setEnvelopeDecay (4);
  source.setEnvelopeMax (0.75f);
  source.setActMode (ActMode::Hold);
  source.setPlayDirection (PlayDirection::Reverse);
  source.setEndAction (EndAction::Bounce);

  Pattern target;
  applyClipSettings (target, clipSettingsFrom (source));

  EXPECT_EQ (target.getSpeedLog2 (), -1);
  EXPECT_FLOAT_EQ (target.getFadeReach (), 0.75f);
  EXPECT_EQ (target.getBridgeBias (), -3);
  EXPECT_FLOAT_EQ (target.getRotate (), 0.25f);
  EXPECT_FLOAT_EQ (target.getReach (), 0.4f);
  EXPECT_FLOAT_EQ (target.getClipTop (), 0.1f);
  EXPECT_FLOAT_EQ (target.getClipBottom (), 0.2f);
  EXPECT_TRUE (target.getMirrorSouth ());
  EXPECT_TRUE (target.getFlat ());
  EXPECT_FLOAT_EQ (target.getFlatElevation (), 0.3f);
  EXPECT_EQ (target.getSpin (), 2);
  EXPECT_EQ (target.getReachLfo (), -3);
  EXPECT_EQ (target.getEnvelopeAttack (), 1);
  EXPECT_EQ (target.getEnvelopeDecay (), 4);
  EXPECT_FLOAT_EQ (target.getEnvelopeMax (), 0.75f);
  EXPECT_EQ (target.getActMode (), ActMode::Hold);
  EXPECT_EQ (target.getPlayDirection (), PlayDirection::Reverse);
  EXPECT_EQ (target.getEndAction (), EndAction::Bounce);
}

// A default-constructed Pattern and a default ClipSettings must agree, or a
// clip file that leaves a field out would load as something other than
// "unset" -- which is what lets a new setting be added without invalidating
// every file already written.
TEST (ClipSettings, DefaultsMatchAFreshPattern)
{
  Pattern fresh;
  Pattern applied;
  applyClipSettings (applied, ClipSettings{});

  EXPECT_EQ (applied.getSpeedLog2 (), fresh.getSpeedLog2 ());
  EXPECT_FLOAT_EQ (applied.getFadeReach (), fresh.getFadeReach ());
  EXPECT_EQ (applied.getBridgeBias (), fresh.getBridgeBias ());
  EXPECT_FLOAT_EQ (applied.getRotate (), fresh.getRotate ());
  EXPECT_FLOAT_EQ (applied.getReach (), fresh.getReach ());
  EXPECT_FLOAT_EQ (applied.getClipTop (), fresh.getClipTop ());
  EXPECT_FLOAT_EQ (applied.getClipBottom (), fresh.getClipBottom ());
  EXPECT_EQ (applied.getMirrorSouth (), fresh.getMirrorSouth ());
  EXPECT_EQ (applied.getFlat (), fresh.getFlat ());
  EXPECT_FLOAT_EQ (applied.getFlatElevation (), fresh.getFlatElevation ());
  EXPECT_EQ (applied.getSpin (), fresh.getSpin ());
  EXPECT_EQ (applied.getReachLfo (), fresh.getReachLfo ());
  EXPECT_EQ (applied.getEnvelopeAttack (), fresh.getEnvelopeAttack ());
  EXPECT_EQ (applied.getEnvelopeDecay (), fresh.getEnvelopeDecay ());
  EXPECT_FLOAT_EQ (applied.getEnvelopeMax (), fresh.getEnvelopeMax ());
  EXPECT_EQ (applied.getActMode (), fresh.getActMode ());
  EXPECT_EQ (applied.getPlayDirection (), fresh.getPlayDirection ());
  EXPECT_EQ (applied.getEndAction (), fresh.getEndAction ());
}

// The phases are deliberately absent: where a spin happens to be at the moment
// of saving is not a setting, it is where the clip got to. Restoring it would
// make a saved clip start mid-turn.
TEST (ClipSettings, RunningPhasesAreNotSettings)
{
  Pattern source;
  source.setSpin (2);
  source.setSpinPhase (0.75f);
  source.setReachLfoPhase (0.4f);

  Pattern target;
  target.setSpinPhase (0.f);
  applyClipSettings (target, clipSettingsFrom (source));

  EXPECT_EQ (target.getSpin (), 2) << "the movement is a setting";
  EXPECT_FLOAT_EQ (target.getSpinPhase (), 0.f) << "where it got to is not";
  EXPECT_FLOAT_EQ (target.getReachLfoPhase (), 0.f);
}

// ── Comparing two of them ────────────────────────────────────────────────

// "Modified" is worked out by comparing a pattern against its clip file, so a
// field left out of the comparison is a control whose changes never show up as
// unsaved -- and are then quietly lost. Every field is walked one at a time
// rather than trusting one composite case.
TEST (ClipSettings, EveryFieldTakesPartInTheComparison)
{
  for (auto const &[name, mutate] : clipSettingsFields ())
    {
      ClipSettings changed;
      mutate (changed);

      EXPECT_NE (changed, ClipSettings{})
          << name << " is not part of the comparison, so turning it would "
                     "never show up as unsaved";
    }
}

TEST (ClipSettings, TwoOfTheSameAreTheSame)
{
  Pattern pattern;
  pattern.setSpin (3);
  pattern.setReach (0.4f);
  pattern.setEndAction (EndAction::Bounce);

  EXPECT_EQ (clipSettingsFrom (pattern), clipSettingsFrom (pattern));
  EXPECT_EQ (ClipSettings{}, ClipSettings{});
}

// Turning a control and turning it back leaves nothing behind. That is the
// difference between comparing and setting a flag: a flag would still say
// "unsaved" after the value came home.
TEST (ClipSettings, TurningSomethingBackLeavesNoTrace)
{
  Pattern pattern;
  auto const before = clipSettingsFrom (pattern);

  pattern.setSpin (4);
  EXPECT_NE (clipSettingsFrom (pattern), before);

  pattern.setSpin (0);
  EXPECT_EQ (clipSettingsFrom (pattern), before);
}

// Drift is worked out by comparing, not by a flag, so a field the comparison
// does not know about is a change that never lights Save.
TEST (ClipSettingsFade, BothNewFieldsCountAsADifference)
{
  ClipSettings a;
  ClipSettings b;
  ASSERT_EQ (a, b);

  b.fadeReach = a.fadeReach + 0.1f;
  EXPECT_NE (a, b);

  b = a;
  b.bridgeBias = 2;
  EXPECT_NE (a, b);
}

// ── sway: the elevation's own slow sweep ─────────────────────────────────

TEST (ClipSettings, SwayGoesToAndFromThePattern)
{
  Pattern pattern;

  ClipSettings settings;
  settings.elevationLfo = -5;
  applyClipSettings (pattern, settings);
  EXPECT_EQ (pattern.getElevationLfo (), -5);

  EXPECT_EQ (clipSettingsFrom (pattern).elevationLfo, -5);
}

TEST (ClipSettings, SwayIsPartOfWhatMakesTwoSettingsDifferent)
{
  // Everything a clip carries is compared, or "unsaved" would stop meaning
  // anything for whichever field was forgotten.
  ClipSettings a;
  ClipSettings b;
  ASSERT_EQ (a, b);

  b.elevationLfo = 3;
  EXPECT_NE (a, b);
}

/** Both of the clip's slow sweeps in one place.
 *
 *  They were applied by hand in three: the engine before it projects, and the
 *  renderer in each of its two paths -- and all three had to agree or the line
 *  would be drawn where the blob is not running. Adding sway would have made
 *  it six. */
TEST (ClipSettings, TheSweepsAreAppliedInOnePlaceForEverybody)
{
  Pattern pattern;
  pattern.setReach (0.5f);
  pattern.setElevationBase (0.5f);

  ElevationParams const set{ pattern.getElevationParams () };

  // Nothing sweeping: exactly what was set, which is what a clip with no
  // modulation has always sent.
  EXPECT_FLOAT_EQ (sweptElevation (set, pattern).reach, set.reach);
  EXPECT_FLOAT_EQ (sweptElevation (set, pattern).elevationBase,
                   set.elevationBase);

  // Each sweep moves its own value and leaves the other alone.
  pattern.setReachLfo (4);
  pattern.setReachLfoPhase (0.25f);
  EXPECT_NE (sweptElevation (set, pattern).reach, set.reach);
  EXPECT_FLOAT_EQ (sweptElevation (set, pattern).elevationBase,
                   set.elevationBase);

  pattern.setReachLfo (0);
  pattern.setElevationLfo (4);
  pattern.setElevationLfoPhase (0.25f);
  EXPECT_FLOAT_EQ (sweptElevation (set, pattern).reach, set.reach);
  EXPECT_NE (sweptElevation (set, pattern).elevationBase, set.elevationBase);

  // And the swept base stays somewhere the sphere can be asked about.
  pattern.setElevationLfoPhase (0.75f);
  auto const swept = sweptElevation (set, pattern).elevationBase;
  EXPECT_GE (swept, 0.f);
  EXPECT_LE (swept, 1.f);
}
