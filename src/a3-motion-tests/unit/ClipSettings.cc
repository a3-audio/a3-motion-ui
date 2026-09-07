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
  // Not 0.5: with a reach of a half that is exactly where the cone runs out
  // of room, and a sway there has nowhere to travel -- which is its own test
  // below, not this one's subject.
  pattern.setElevationBase (0.2f);

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
  EXPECT_NE (sweptElevation (set, pattern).elevationBase, set.elevationBase);

  // And the sway leaves the reach alone. The two used to be coupled -- the
  // reach was held to the room in front of the swept base -- and that is the
  // coupling that shrank a figure to a splinter whenever elevation was pushed
  // near a wall. See AReachIsLeftWhereTheHandPutItAndWrapsOverTheWall.
  EXPECT_FLOAT_EQ (sweptElevation (set, pattern).reach, set.reach);

  // And the swept base stays somewhere the sphere can be asked about.
  pattern.setElevationLfoPhase (0.75f);
  auto const swept = sweptElevation (set, pattern).elevationBase;
  EXPECT_GE (swept, 0.f);
  EXPECT_LE (swept, 1.f);
}

// ── squeeze: the figure pressed flat in its own plane ────────────────────

TEST (ClipSettings, TheSqueezesGoToAndFromThePattern)
{
  Pattern pattern;

  ClipSettings settings;
  settings.squeezeX = 0.4f;
  settings.squeezeY = -0.75f;
  applyClipSettings (pattern, settings);
  EXPECT_FLOAT_EQ (pattern.getSqueezeX (), 0.4f);
  EXPECT_FLOAT_EQ (pattern.getSqueezeY (), -0.75f);

  EXPECT_FLOAT_EQ (clipSettingsFrom (pattern).squeezeX, 0.4f);
  EXPECT_FLOAT_EQ (clipSettingsFrom (pattern).squeezeY, -0.75f);
}

TEST (ClipSettings, TheSqueezesArePartOfWhatMakesTwoSettingsDifferent)
{
  ClipSettings a;
  ClipSettings b;
  ASSERT_EQ (a, b);

  b.squeezeX = 0.3f;
  EXPECT_NE (a, b);

  b = a;
  b.squeezeY = -0.3f;
  EXPECT_NE (a, b);
}

// The ends of the travel are the ends of the travel wherever the value comes
// from -- a script, a file, or an encoder that has been turned a long way.
TEST (ClipSettings, APatternHoldsASqueezeToItsTravel)
{
  Pattern pattern;

  pattern.setSqueezeX (7.f);
  EXPECT_FLOAT_EQ (pattern.getSqueezeX (), 1.f);

  pattern.setSqueezeY (-7.f);
  EXPECT_FLOAT_EQ (pattern.getSqueezeY (), -1.f);
}

// ── The sway keeps the figure somewhere it fits ──────────────────────────

/** The base travels the whole way, pole to pole. The pad is wrapped around it
 *  as a cap, so there is no end for a cone to run out of room at -- and the
 *  base being at a pole is the ordinary case, not the degenerate one. */
TEST (ClipSettings, TheSwaySweepsTheBaseFromEndToEnd)
{
  Pattern pattern;
  pattern.setReach (0.5f);
  pattern.setElevationBase (0.f);
  pattern.setElevationLfo (4);

  ElevationParams const set{ pattern.getElevationParams () };

  auto furthest = 0.f;
  for (int i = 0; i <= 32; ++i)
    {
      pattern.setElevationLfoPhase (static_cast<float> (i) / 32.f);
      auto const base = sweptElevation (set, pattern).elevationBase;

      EXPECT_GE (base, 0.f);
      EXPECT_LE (base, 1.f);
      furthest = std::max (furthest, base);
    }

  EXPECT_GT (furthest, 0.9f) << "a positive sway travels to the far pole";
}



// ── The swell stays in the room ─────────────────────────────────────────

/** The swell breathes the figure's size and nothing else.
 *
 *  It is allowed to swell past a pole: a reach of one puts the outer edge a
 *  whole half-turn from the base, and running over the wall is what the band
 *  model is for. What it may not do is change the sign -- swept signed, a
 *  reach set upwards would pass through nothing and come out spreading
 *  downwards, which is the figure turning inside out rather than breathing.
 *
 *  Only a clip stops it, which is TheSwellStaysInsideTheClips' subject.
 */
TEST (ClipSettings, TheSwellBreathesPastThePole)
{
  Pattern pattern;
  pattern.setReachLfo (2);          // swelling towards a full reach
  pattern.setReachLfoPhase (0.5f);  // and standing at the far end of it

  ElevationParams params;
  params.reach = 0.6f;
  params.elevationBase = 0.35f;

  auto const swept = sweptElevation (params, pattern);

  EXPECT_GT (swept.reach, params.reach) << "the swell has to move something";
  EXPECT_GT (swept.reach, 1.f - params.elevationBase)
      << "the pole held it back, and a pole is not a cut";
  EXPECT_LE (swept.reach, 1.f) << "and it is still a reach";
}

/** And upwards, where the old rule held it to the base itself. */
TEST (ClipSettings, TheSwellBreathesPastTheCeilingToo)
{
  Pattern pattern;
  pattern.setReachLfo (2);
  pattern.setReachLfoPhase (0.5f);

  ElevationParams params;
  params.reach = -0.3f;
  params.elevationBase = 0.4f;

  auto const swept = sweptElevation (params, pattern);

  EXPECT_LT (swept.reach, 0.f) << "the swell must not turn the figure over";
  EXPECT_LT (swept.reach, -params.elevationBase)
      << "the ceiling held it back, and a pole is not a cut";
  EXPECT_GE (swept.reach, -1.f);
}

// ── Where a double tap puts the reach ───────────────────────────────────

/** Twelve o'clock, like every other bipolar knob in the bar.
 *
 *  The knob is filled from its middle, so twelve o'clock is where the eye
 *  reads "home" -- and a reset that landed anywhere else read as not having
 *  worked, whatever number was behind it. It cost a choice to say so: at a
 *  reach of nothing the figure lies flat on one latitude, so the safe thing
 *  to press is also the thing that flattens the trajectory. That is what the
 *  knob has said all along; it is not the reset's place to disagree with it.
 *
 *  Not the file default of 0.5 either, which is what a *fresh clip* carries
 *  -- a different question from where a knob goes back to.
 */
TEST (ClipSettings, TwoTapsOnTheReachGoToTwelveOClock)
{
  for (float from : { 0.9f, 0.1f, 0.f, -0.1f, -0.9f })
    EXPECT_FLOAT_EQ (defaultReach (from), 0.f) << "from " << from;

  // And a fresh clip still spreads: the two are not the same number.
  EXPECT_GT (ClipSettings{}.reach, 0.f);
}

// ── Where a double tap puts the elevation line ──────────────────────────

/** The middle of the range the control actually has.
 *
 *  That is the rule the rest of the bar's knobs already follow, and for the
 *  elevation line the range is the circle it is drawn in: its middle is the
 *  equator, ear height, which is also the one height a hand reaching for
 *  "neutral" mid-set means. Not the clip default of zero -- that is straight
 *  overhead, which is a place to put a sound, not a place to come back to.
 */
TEST (ClipSettings, TheElevationLineGoesBackToEarHeight)
{
  EXPECT_FLOAT_EQ (defaultElevationBase (0.f, 0.f), 0.5f);
}

/** And to the middle of what the clips have left of it, so a double tap never
 *  puts the line somewhere the sound may not go. */
TEST (ClipSettings, TheElevationLineGoesBackInsideTheClips)
{
  // Ceiling a third down, floor a fifth up: the middle of what is left.
  EXPECT_FLOAT_EQ (defaultElevationBase (0.3f, 0.2f), 0.55f);

  // Clips pushed past each other pin it to where they crossed, the same rule
  // sweptElevation() holds the base by.
  EXPECT_FLOAT_EQ (defaultElevationBase (0.8f, 0.6f), 0.6f);
}

/** A hand-set reach stands as long as it fits. */
TEST (ClipSettings, AReachThatFitsIsLeftWhereItWasPut)
{
  Pattern pattern;

  ElevationParams params;
  params.reach = 0.4f;
  params.elevationBase = 0.5f;

  EXPECT_FLOAT_EQ (sweptElevation (params, pattern).reach, 0.4f);
}

/** And it stays there when it no longer fits, because not fitting is the
 *  point.
 *
 *  The band model exists so that a figure runs *over the outer wall*: what
 *  reaches a pole carries on past it instead of stopping there. Holding the
 *  reach to `1 - base` forbade exactly that. The cost was paid where elevation
 *  is most often left -- based near the floor, a reach of 0.65 came out as
 *  0.108, so the figure shrank to a splinter while the run from the pad's
 *  middle to the pole kept its full length, and that run was then the whole
 *  picture.
 *
 *  Only a clip bounds the reach now: a cut is a hard clamp on where the sound
 *  may go, and a figure pushed past one piles onto it. A pole is not a cut.
 */
TEST (ClipSettings, AReachIsLeftWhereTheHandPutItAndWrapsOverTheWall)
{
  Pattern pattern;

  ElevationParams params;
  params.reach = 0.65f;
  params.elevationBase = 0.892f;

  EXPECT_FLOAT_EQ (sweptElevation (params, pattern).reach, 0.65f)
      << "the wall is not a wall the reach has to stop at";

  // And upwards, where the old rule held it to the base itself.
  params.reach = -0.65f;
  params.elevationBase = 0.108f;

  EXPECT_FLOAT_EQ (sweptElevation (params, pattern).reach, -0.65f);
}

/** Nor does a swaying base take it with it. */
TEST (ClipSettings, ASwayingBaseLeavesTheReachAlone)
{
  Pattern pattern;
  pattern.setElevationLfo (1);         // swaying towards the floor
  pattern.setElevationLfoPhase (0.4f); // most of the way there, not at it

  ElevationParams params;
  params.reach = 0.28f;
  params.elevationBase = 0.5f;

  auto const swept = sweptElevation (params, pattern);

  ASSERT_GT (swept.elevationBase, 0.8f) << "the sway has to have moved it";
  EXPECT_FLOAT_EQ (swept.reach, 0.28f);
}

/** And the clips bound it too. They are a hard clamp -- a point pushed past
 *  one keeps its bearing and gives up its height -- so a swell that sweeps
 *  into one is not opening the figure out, it is piling it onto the cut. */
TEST (ClipSettings, TheSwellStaysInsideTheClips)
{
  Pattern pattern;
  pattern.setReachLfo (2);
  pattern.setReachLfoPhase (0.5f);

  ElevationParams params;
  params.reach = 0.3f;
  params.elevationBase = 0.1f;
  params.clipBottom = 0.4f; // nothing below 0.6

  auto const swept = sweptElevation (params, pattern);

  EXPECT_LE (swept.reach, 0.6f - params.elevationBase + 1e-5f)
      << "it swept the figure onto the cut";
  EXPECT_GT (swept.reach, params.reach) << "and it still has room to move";
}

/** The sway turns back at the clip, not at the pole.
 *
 *  The clips are a hard clamp on where the sound may go, so a sway that swept
 *  past one spent part of every cycle standing on the cut while the number
 *  behind it carried on -- the movement stopped and the reading did not, which
 *  reads as the sway travelling through the part of the room that was taken
 *  away.
 */
TEST (ClipSettings, TheSwayTurnsBackAtTheClipNotAtThePole)
{
  Pattern pattern;
  pattern.setElevationLfo (2);      // swaying towards the floor
  pattern.setElevationLfoPhase (0.5f); // and standing at the far end of it

  ElevationParams params;
  params.reach = 0.1f;
  params.elevationBase = 0.4f;
  params.clipBottom = 0.35f;        // nothing below 0.65

  auto const swept = sweptElevation (params, pattern);

  EXPECT_GT (swept.elevationBase, params.elevationBase)
      << "the sway has to move it";
  EXPECT_LE (swept.elevationBase, 0.65f + 1e-5f)
      << "it swayed into the part of the room the clip took away";
  EXPECT_NEAR (swept.elevationBase, 0.65f, 1e-4f)
      << "and it should reach the cut, not stop short of it";
}

/** The other way round the same. */
TEST (ClipSettings, TheSwayTurnsBackAtTheCeilingToo)
{
  Pattern pattern;
  pattern.setElevationLfo (-2);
  pattern.setElevationLfoPhase (0.5f);

  ElevationParams params;
  params.elevationBase = 0.6f;
  params.clipTop = 0.3f;

  EXPECT_NEAR (sweptElevation (params, pattern).elevationBase, 0.3f, 1e-4f);
}

/** With no clips it still travels the whole way, pole to pole: the bound is
 *  the cut, and where there is no cut there is no bound. */
TEST (ClipSettings, WithoutClipsTheSwayStillReachesThePole)
{
  Pattern pattern;
  pattern.setElevationLfo (2);
  pattern.setElevationLfoPhase (0.5f);

  ElevationParams params;
  params.elevationBase = 0.4f;

  EXPECT_NEAR (sweptElevation (params, pattern).elevationBase, 1.f, 1e-4f);
}

/** The base is held inside the clips, not merely bounded by them.
 *
 *  The control that sets it keeps it in the band, but the clips move
 *  afterwards: turn the ceiling down past where the base already stands and
 *  the base is outside the room it is supposed to be in. Every point of the
 *  figure then clamps onto the cut, and a whole trajectory piled onto one
 *  height is a straight line drawn across the picture -- which is what it
 *  looked like, and what it was.
 */
TEST (ClipSettings, TheBaseIsHeldInsideTheClips)
{
  Pattern pattern;

  ElevationParams params;
  params.reach = 0.2f;
  params.elevationBase = 0.f;   // set while there was no ceiling
  params.clipTop = 0.3f;        // and then the ceiling came down past it

  auto const swept = sweptElevation (params, pattern);

  EXPECT_GE (swept.elevationBase, params.clipTop - 1e-5f)
      << "the base is outside the room the clips leave";
  EXPECT_LE (swept.elevationBase, 1.f - params.clipBottom + 1e-5f);
}

/** And from the other end. */
TEST (ClipSettings, TheBaseIsHeldOffTheFloorToo)
{
  Pattern pattern;

  ElevationParams params;
  params.elevationBase = 1.f;
  params.clipBottom = 0.25f;

  EXPECT_LE (sweptElevation (params, pattern).elevationBase, 0.75f + 1e-5f);
}

/** A base already inside the band is left exactly where it was put. */
TEST (ClipSettings, ABaseInsideTheClipsIsNotMoved)
{
  Pattern pattern;

  ElevationParams params;
  params.elevationBase = 0.4f;
  params.clipTop = 0.2f;
  params.clipBottom = 0.2f;

  EXPECT_FLOAT_EQ (sweptElevation (params, pattern).elevationBase, 0.4f);
}
