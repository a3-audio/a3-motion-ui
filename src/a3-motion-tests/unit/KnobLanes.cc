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

#include <a3-motion-engine/KnobLanes.hh>

#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace a3;

// Step 3c of the REC workflow (2026-09-26): the Motion and Elevation knobs
// turned during a take are recorded as lanes and play back with the clip.

namespace
{
constexpr long long lap = 16;

/** One pass of a knob through `lap` ticks: held on [heldFrom, heldTo),
 *  turned to `value` there, standing at `rest` elsewhere. */
void
pass (KnobRecorder &recorder, KnobLane &lane, RecMode mode, long long from,
      long long heldFrom, long long heldTo, float value, float rest)
{
  for (auto t = from; t < from + lap; ++t)
    {
      auto const held = t >= heldFrom && t < heldTo;
      recorder.recordTick (lane, mode, held, held ? value : rest, t, lap);
    }
}
}

TEST (KnobLanes, AnEmptyLaneHasNoValue)
{
  KnobLane lane (lap);
  EXPECT_TRUE (lane.empty ());
  EXPECT_FALSE (lane.at (3.0).has_value ());
}

// Touch: the knob writes while a hand holds it, and between what it wrote
// the lane holds the last value -- a knob turned and let go stays there.
TEST (KnobLanes, TouchWritesWhileHeld)
{
  KnobLane lane (lap);
  KnobRecorder recorder;
  pass (recorder, lane, RecMode::Touch, 0, 2, 5, 0.7f, 0.1f);

  ASSERT_FALSE (lane.empty ());
  EXPECT_FLOAT_EQ (*lane.at (3.0), 0.7f);
  EXPECT_FLOAT_EQ (*lane.at (9.0), 0.7f) << "the lane holds after the hand";
}

TEST (KnobLanes, TouchWritesNothingUnheld)
{
  KnobLane lane (lap);
  KnobRecorder recorder;
  pass (recorder, lane, RecMode::Touch, 0, 99, 99, 0.7f, 0.1f);
  EXPECT_TRUE (lane.empty ());
}

// A second Touch pass leaves what it does not touch as the first wrote it.
TEST (KnobLanes, ALaterTouchPassLeavesTheRestAlone)
{
  KnobLane lane (lap);
  KnobRecorder recorder;
  pass (recorder, lane, RecMode::Touch, 0, 0, 16, 0.2f, 0.2f);
  pass (recorder, lane, RecMode::Touch, lap, lap + 4, lap + 6, 0.9f, 0.2f);

  EXPECT_FLOAT_EQ (*lane.at (5.0), 0.9f);
  EXPECT_FLOAT_EQ (*lane.at (10.0), 0.2f);
}

// Latch: from the touch on, the knob's value is written to the end of the
// lap it was let go in, touched or not.
TEST (KnobLanes, LatchWritesOnAfterTheLiftToTheLapsEnd)
{
  // A take over one that is already there: a recorder of its own.
  KnobLane lane (lap);
  KnobRecorder first, second;
  pass (first, lane, RecMode::Touch, 0, 0, 16, 0.2f, 0.2f);
  pass (second, lane, RecMode::Latch, 0, 4, 6, 0.9f, 0.9f);

  EXPECT_FLOAT_EQ (*lane.at (2.0), 0.2f) << "before the touch";
  EXPECT_FLOAT_EQ (*lane.at (12.0), 0.9f) << "after the lift, same lap";
}

// Write: the whole pass is written, touched or not.
TEST (KnobLanes, WriteWritesTheWholePass)
{
  KnobLane lane (lap);
  KnobRecorder first, second;
  pass (first, lane, RecMode::Touch, 0, 0, 16, 0.2f, 0.2f);
  pass (second, lane, RecMode::Write, 0, 99, 99, 0.5f, 0.5f);

  for (int t = 0; t < lap; ++t)
    EXPECT_FLOAT_EQ (*lane.at (t), 0.5f) << "tick " << t;
}

// For the file: only where the value changes, and read back the same.
TEST (KnobLanes, ChangePointsRoundTrip)
{
  KnobLane lane (lap);
  KnobRecorder recorder;
  pass (recorder, lane, RecMode::Touch, 0, 2, 5, 0.7f, 0.1f);
  pass (recorder, lane, RecMode::Touch, lap, lap + 8, lap + 11, 0.3f, 0.1f);

  auto const points = lane.changePoints ();
  EXPECT_LE (points.size (), 4u) << "every tick written out";

  auto const back = KnobLane::fromChangePoints (points, lap);
  for (int t = 0; t < lap; ++t)
    EXPECT_EQ (back.at (t), lane.at (t)) << "tick " << t;
}

// The names are what the clip file stores: one per knob, none twice.
TEST (KnobLanes, EachKnobHasItsOwnName)
{
  std::set<std::string> names;
  for (int k = 0; k < numKnobs; ++k)
    names.insert (knobName (static_cast<Knob> (k)));
  EXPECT_EQ (names.size (), static_cast<std::size_t> (numKnobs));
}
