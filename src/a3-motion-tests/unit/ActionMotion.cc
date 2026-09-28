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

#include <a3-motion-engine/ActionMotion.hh>
#include <a3-motion-engine/ActionScript.hh>
#include <a3-motion-engine/TempoLfo.hh>

#include <set>
#include <string>

using namespace a3;

// What a button puts on the clip, beside the script (2026-09-28): the MOTION
// tile's values, the button's own where they were turned. A value nobody
// turned keeps coming from the script -- or, if the script does not name it
// either, from the clip.

namespace
{
/** A value in range for each parameter, and not the default, so a round trip
 *  that dropped it would show. */
float
aValueFor (MotionParam param)
{
  switch (param)
    {
    case MotionParam::Spin: return 3.f;
    case MotionParam::Rotate: return 0.25f;
    case MotionParam::Swell: return -2.f;
    case MotionParam::Reach: return -0.5f;
    case MotionParam::StretchX: return 4.f;
    case MotionParam::SqueezeX: return 0.5f;
    case MotionParam::StretchY: return -4.f;
    case MotionParam::SqueezeY: return -0.25f;
    case MotionParam::Sway: return 1.f;
    case MotionParam::Elevation: return 0.75f;
    case MotionParam::ClipTop: return 0.125f;
    case MotionParam::ClipBottom: return 0.375f;
    case MotionParam::TiltSweep: return 2.f;
    case MotionParam::Tilt: return 0.5f;
    case MotionParam::RollSweep: return -1.f;
    case MotionParam::Roll: return -0.5f;
    case MotionParam::Speed: return -2.f;
    case MotionParam::Direction: return 1.f;
    case MotionParam::EndAction: return 2.f;
    }
  return 0.f;
}
}

TEST (ActionMotion, NothingTurnedIsEmpty)
{
  MotionOverrides motion;
  EXPECT_TRUE (motion.empty ());
  for (auto const param : motionParamOrder)
    EXPECT_FALSE (motion.get (param).has_value ());
}

TEST (ActionMotion, EveryParameterIsListedOnce)
{
  std::set<int> seen;
  for (auto const param : motionParamOrder)
    EXPECT_TRUE (seen.insert (static_cast<int> (param)).second)
        << motionParamKey (param);
  EXPECT_EQ (static_cast<int> (seen.size ()), numMotionParams);
}

// Every parameter goes onto settings and comes back as it went in.
TEST (ActionMotion, EveryParameterRoundTripsThroughSettings)
{
  for (auto const param : motionParamOrder)
    {
      auto const value = aValueFor (param);
      auto const settings = withMotionValue (ClipSettings{}, param, value);
      EXPECT_FLOAT_EQ (motionValueOf (settings, param), value)
          << motionParamKey (param);
    }
}

// A value off the end of its range lands on the end, as a script's does.
TEST (ActionMotion, ValuesAreHeldInsideTheirRange)
{
  ClipSettings s;
  EXPECT_EQ (withMotionValue (s, MotionParam::Spin, 99.f).spin, lfoMaxStep);
  EXPECT_EQ (withMotionValue (s, MotionParam::Speed, 99.f).speedLog2,
             speedLog2Max);
  EXPECT_EQ (withMotionValue (s, MotionParam::Speed, -99.f).speedLog2,
             speedLog2Min);
  EXPECT_FLOAT_EQ (withMotionValue (s, MotionParam::Reach, 5.f).reach, 1.f);
  EXPECT_FLOAT_EQ (withMotionValue (s, MotionParam::ClipTop, -1.f).clipTop,
                   0.f);
  EXPECT_EQ (withMotionValue (s, MotionParam::Direction, 7.f).direction,
             PlayDirection::Random);
  EXPECT_EQ (withMotionValue (s, MotionParam::EndAction, -3.f).endAction,
             EndAction::Loop);
}

// Every parameter can be named in a script, so "does the script set it" has
// an answer for each.
TEST (ActionMotion, EveryParameterIsAScriptName)
{
  auto const names = actionScriptNames ();
  for (auto const param : motionParamOrder)
    EXPECT_TRUE (names.contains (motionParamScriptName (param)))
        << motionParamScriptName (param);
}

TEST (ActionMotion, TurningSetsAndDoubleTapUnsets)
{
  MotionOverrides motion;
  motion.set (MotionParam::Spin, 0.f);
  EXPECT_FALSE (motion.empty ());
  ASSERT_TRUE (motion.get (MotionParam::Spin).has_value ());
  // Zero is a value the button chose, not "nothing".
  EXPECT_FLOAT_EQ (*motion.get (MotionParam::Spin), 0.f);

  motion.unset (MotionParam::Spin);
  EXPECT_TRUE (motion.empty ());
}

// ── Firing: the script, then the button's motion, then its feel ─────────

TEST (ActionMotion, AValueNobodyTurnedComesFromTheScript)
{
  ClipSettings base;
  base.spin = 1;
  auto const fired
      = resolveActionAt ("~spin = 5;", base, 7, MotionOverrides{}, ActionFeel{});
  EXPECT_EQ (fired.spin, 5);
}

TEST (ActionMotion, AValueNeitherSetsComesFromTheClip)
{
  ClipSettings base;
  base.reach = -0.3f;
  auto const fired
      = resolveActionAt ("~spin = 5;", base, 7, MotionOverrides{}, ActionFeel{});
  EXPECT_FLOAT_EQ (fired.reach, -0.3f);
}

TEST (ActionMotion, ATurnedValueWinsOverTheScript)
{
  MotionOverrides motion;
  motion.set (MotionParam::Spin, -2.f);
  auto const fired = resolveActionAt ("~spin = 5;\n~reach = 0.9;",
                                      ClipSettings{}, 7, motion, ActionFeel{});
  EXPECT_EQ (fired.spin, -2);
  EXPECT_FLOAT_EQ (fired.reach, 0.9f) << "the script's other lines still hold";
}

// The script is worked out first and does not see the button's values: a
// line reading ~spin reads the clip's, and the override then replaces it.
TEST (ActionMotion, TheScriptRunsBeforeTheOverrides)
{
  ClipSettings base;
  base.spin = 2;
  MotionOverrides motion;
  motion.set (MotionParam::Spin, 6.f);
  auto const fired = resolveActionAt ("~spin = ~spin + 1;\n~swell = ~spin;",
                                      base, 7, motion, ActionFeel{});
  EXPECT_EQ (fired.spin, 6);
  EXPECT_EQ (fired.reachLfo, 3) << "swell read the script's spin, not the turn";
}

TEST (ActionMotion, TheFeelGoesOnLast)
{
  MotionOverrides motion;
  motion.set (MotionParam::Speed, 1.f);
  ActionFeel feel;
  feel.envelopeAttack = 6;
  feel.actMode = ActMode::Hold;
  auto const fired = resolveActionAt ("~attack = 0;\n~act = \\oneshot;",
                                      ClipSettings{}, 7, motion, feel);
  EXPECT_EQ (fired.speedLog2, 1);
  EXPECT_EQ (fired.envelopeAttack, 6);
  EXPECT_EQ (fired.actMode, ActMode::Hold);
}

TEST (ActionMotion, UnsettingHandsTheValueBackToTheScript)
{
  MotionOverrides motion;
  motion.set (MotionParam::Spin, -2.f);
  motion.unset (MotionParam::Spin);
  auto const fired
      = resolveActionAt ("~spin = 5;", ClipSettings{}, 7, motion, ActionFeel{});
  EXPECT_EQ (fired.spin, 5);
}

// ── What the tile shows ─────────────────────────────────────────────────

TEST (ActionMotion, EachValueSaysWhereItComesFrom)
{
  ClipSettings base;
  base.tilt = 0.4f;
  MotionOverrides motion;
  motion.set (MotionParam::Reach, -0.6f);

  auto const shown
      = motionShownFor ("~spin = 5;\n~reach = 0.9;", base, 7, motion);

  auto const at = [&shown] (MotionParam p) {
    return shown[static_cast<size_t> (p)];
  };
  EXPECT_EQ (at (MotionParam::Spin).source, MotionSource::Script);
  EXPECT_FLOAT_EQ (at (MotionParam::Spin).value, 5.f);
  EXPECT_EQ (at (MotionParam::Reach).source, MotionSource::Button);
  EXPECT_FLOAT_EQ (at (MotionParam::Reach).value, -0.6f);
  EXPECT_EQ (at (MotionParam::Tilt).source, MotionSource::Clip);
  EXPECT_FLOAT_EQ (at (MotionParam::Tilt).value, 0.4f)
      << "unset shows the clip's own value, as a hint";
}

// A line the script got wrong sets nothing, so it does not claim the value.
TEST (ActionMotion, ABrokenLineSetsNothing)
{
  auto const shown = motionShownFor ("~spin = ;", ClipSettings{}, 7, {});
  EXPECT_EQ (shown[static_cast<size_t> (MotionParam::Spin)].source,
             MotionSource::Clip);
}

TEST (ActionScript, SaysWhichNamesItSet)
{
  auto const result
      = runActionScript ("~spin = 3;\n// ~reach = 1;\n~dir = \\reverse;\n~nope = 1;",
                         ClipSettings{}, 1);
  EXPECT_TRUE (result.assigned.contains ("spin"));
  EXPECT_TRUE (result.assigned.contains ("dir"));
  EXPECT_FALSE (result.assigned.contains ("reach"));
  EXPECT_FALSE (result.assigned.contains ("nope"));
}
