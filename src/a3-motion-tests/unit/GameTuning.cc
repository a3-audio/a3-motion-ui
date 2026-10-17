/*

  A3 Motion UI
  Copyright (C) 2026 Raphael Eismann

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

#include <a3-motion-engine/flight/FlightTuning.hh>
#include <a3-motion-engine/flight/GameTuning.hh>

using namespace a3;

// The families' table holds together: the values that must agree with
// another value agree, and the defaults keep the rules they stand for.

TEST (GameTuning, TheFamiliesKeepTheirFiguresInsideTheRoom)
{
  GameTuning const tuning;
  FlightTuning const flight;
  EXPECT_FLOAT_EQ (tuning.slapSideRadius, tuning.hideRadius);
  EXPECT_LT (tuning.slapSideRadius, flight.rimSoft);
  EXPECT_LT (tuning.grazeOuterRadius, flight.rimSoft);
  EXPECT_LT (tuning.grazeInnerRadius, tuning.grazeOuterRadius);
  EXPECT_LT (tuning.climbTopRadius, flight.orbitRadius);
  EXPECT_GT (tuning.slapClearSide, 2.f * flight.separationSoftening);
}

TEST (GameTuning, TheFamiliesHearWhatAHeardBendIs)
{
  GameTuning const tuning;
  EXPECT_FLOAT_EQ (tuning.grazeMinSweepDegrees, tuning.heardBendDegrees);
  EXPECT_FLOAT_EQ (tuning.encircleMinDegrees, tuning.heardBendDegrees);
}

TEST (GameTuning, TheDefaultSlapKeepsTheEventBudget)
{
  GameTuning const tuning;
  EXPECT_GE (tuning.slapBeatsPerSide, tuning.eventSpacingBeats);
  EXPECT_LE (tuning.slapGlideBeats, 0.25f) << "a sixteenth of a 4/4 bar reads as a step";
  EXPECT_GT (tuning.slapVerticalRateFactor, 0.f);
  EXPECT_LE (tuning.slapVerticalRateFactor, 1.f);
  EXPECT_LE (tuning.hardDropBars, tuning.slapBars);
}

TEST (GameTuning, TheClimbStartsAtCruiseAndNeverPassesALapABar)
{
  GameTuning const tuning;
  EXPECT_FLOAT_EQ (tuning.climbStartLapBars, FlightTuning{}.orbitLapBars);
  EXPECT_GE (tuning.climbFastestLapBars, 1.f);
  EXPECT_GE (tuning.climbMinBars, 2 * tuning.climbDoubleEveryBars)
      << "the shortest build that holds a doubling";
}

TEST (GameTuning, AHardGrooveIsLouderThanASoftOne)
{
  GameTuning const tuning;
  EXPECT_GT (tuning.hardEnergy, tuning.softEnergy);
  EXPECT_GT (tuning.softEnergy, tuning.quietEnergy);
}
