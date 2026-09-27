/*

  A3 Motion UI
  Copyright (C) 2026 Patric Schmitz

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

#include <a3-motion-engine/ClipSettings.hh>
#include <a3-motion-engine/KnobLanes.hh>
#include <a3-motion-engine/Pattern.hh>
#include <a3-motion-engine/TrajectoryShaping.hh>

using namespace a3;

namespace
{

constexpr long long lap = 16;

// A Pattern holds a mutex and cannot be returned; one lap is made in place.
struct OneLap
{
  OneLap () { pattern.resize (lap); }
  Pattern pattern;
};

void
recordLap (Pattern &pattern, KnobRecorders &recorders, RecMode mode)
{
  for (long long t = 0; t < lap; ++t)
    pattern.recordKnobs (recorders, mode, t, lap);
}

KnobLanes
reachAt (float value)
{
  KnobLanes lanes;
  for (auto &lane : lanes)
    lane = KnobLane (lap);
  lanes[static_cast<std::size_t> (Knob::Reach)].write (0, value);
  return lanes;
}

}

TEST (KnobAutomation, AKnobReadsItsSetting)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setRotate (0.25f);
  pattern.setSpin (3);
  pattern.setElevationBase (0.4f);

  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Rotate), 0.25f);
  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Spin), 3.f);
  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Elevation), 0.4f);
}

TEST (KnobAutomation, AHeldKnobIsRecordedIntoItsLane)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setReach (0.7f);
  pattern.setKnobHeld (Knob::Reach, true);

  KnobRecorders recorders;
  recordLap (pattern, recorders, RecMode::Touch);

  auto const lanes = pattern.getLanes ();
  auto const reach = lanes[static_cast<std::size_t> (Knob::Reach)].at (3.0);
  ASSERT_TRUE (reach.has_value ());
  EXPECT_FLOAT_EQ (*reach, 0.7f);
  EXPECT_TRUE (lanes[static_cast<std::size_t> (Knob::Rotate)].empty ())
      << "a knob nobody held has nothing to play back";
}

TEST (KnobAutomation, APlayedLaneOverridesTheSetValue)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setReach (0.8f);
  pattern.setLanes (reachAt (0.3f));

  pattern.playKnobs (2.0);

  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Reach), 0.3f);
  EXPECT_FLOAT_EQ (pattern.getReach (), 0.8f)
      << "the lane plays over the setting, it does not turn the knob";
}

TEST (KnobAutomation, TheHandWinsWhileItHolds)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setReach (0.8f);
  pattern.setLanes (reachAt (0.3f));
  pattern.setKnobHeld (Knob::Reach, true);

  pattern.playKnobs (2.0);
  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Reach), 0.8f);

  pattern.setKnobHeld (Knob::Reach, false);
  pattern.playKnobs (3.0);
  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Reach), 0.3f);
}

TEST (KnobAutomation, AClipWithoutLanesPlaysItsSettings)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setReach (0.8f);

  pattern.playKnobs (2.0);

  EXPECT_FALSE (pattern.hasLanes ());
  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Reach), 0.8f);
}

TEST (KnobAutomation, ClearedLanesStopPlaying)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setReach (0.8f);
  pattern.setLanes (reachAt (0.3f));
  pattern.playKnobs (2.0);

  pattern.clearLanes ();
  pattern.playKnobs (3.0);

  EXPECT_FALSE (pattern.hasLanes ());
  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Reach), 0.8f);
}

TEST (KnobAutomation, TheShapingFollowsAPlayedRotate)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  auto lanes = reachAt (0.5f);
  lanes[static_cast<std::size_t> (Knob::Rotate)].write (0, 0.25f);
  pattern.setLanes (lanes);

  pattern.playKnobs (1.0);

  EXPECT_NEAR (shapingOf (pattern).turns, 0.25f, 1e-6f);
}

TEST (KnobAutomation, TheElevationFollowsAPlayedBase)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setElevationBase (0.1f);
  auto lanes = reachAt (0.5f);
  lanes[static_cast<std::size_t> (Knob::Elevation)].write (0, 0.4f);
  pattern.setLanes (lanes);

  pattern.playKnobs (1.0);

  auto const params = sweptElevation (pattern.getElevationParams (), pattern);
  EXPECT_NEAR (params.elevationBase, 0.4f, 1e-6f);
  EXPECT_NEAR (params.reach, 0.5f, 1e-6f);
}

TEST (KnobAutomation, ClearingOneLaneLeavesTheOthers)
{
  OneLap oneLap;
  auto &pattern = oneLap.pattern;
  pattern.setReach (0.8f);
  auto lanes = reachAt (0.3f);
  lanes[static_cast<std::size_t> (Knob::Rotate)].write (0, 0.25f);
  pattern.setLanes (lanes);
  pattern.playKnobs (2.0);

  pattern.clearLane (Knob::Reach);

  EXPECT_FALSE (pattern.hasLane (Knob::Reach));
  EXPECT_TRUE (pattern.hasLane (Knob::Rotate));
  EXPECT_FLOAT_EQ (pattern.getKnob (Knob::Reach), 0.8f)
      << "the setting plays again at once, not on the next tick";
}
