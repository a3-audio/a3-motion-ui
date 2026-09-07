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

#include <a3-motion-engine/Pattern.hh>

using namespace a3;

// Punch-out has to tell "never touched" from "touched, and happens to hold a
// position that looks like nothing". Only the pattern can know which ticks a
// recording actually wrote, so it keeps the mask alongside the positions.

TEST (PatternWrittenTicks, AFreshPatternHasWrittenNothing)
{
  Pattern pattern;
  pattern.resize (16);

  for (index_t tick = 0; tick < pattern.getNumTicks (); ++tick)
    EXPECT_FALSE (pattern.isTickWritten (tick)) << "tick " << tick;
}

TEST (PatternWrittenTicks, SettingATickMarksIt)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (2, Pos::fromCartesian (0.1f, 0.2f, 0.f));

  EXPECT_TRUE (pattern.isTickWritten (2));
  EXPECT_FALSE (pattern.isTickWritten (1));
}

TEST (PatternWrittenTicks, TheMaskHasOneEntryPerTick)
{
  Pattern pattern;
  pattern.resize (16);

  EXPECT_EQ (pattern.writtenTicks ().size (), pattern.getNumTicks ());
}

TEST (PatternWrittenTicks, ClearingForgetsThem)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, Pos::fromCartesian (0.1f, 0.2f, 0.f));
  pattern.clearWrittenTicks ();

  EXPECT_FALSE (pattern.isTickWritten (0));
  EXPECT_EQ (pattern.writtenTicks ().size (), pattern.getNumTicks ())
      << "forgotten, not thrown away";
}

// A pattern loaded from disk was written in full by whoever recorded it — the
// mask is about this session's recording, so a resize starts it over.
TEST (PatternWrittenTicks, ResizingStartsTheMaskOver)
{
  Pattern pattern;
  pattern.resize (16);
  pattern.setTick (0, Pos::fromCartesian (0.1f, 0.2f, 0.f));
  pattern.resize (32);

  EXPECT_EQ (pattern.writtenTicks ().size (), pattern.getNumTicks ());
  EXPECT_FALSE (pattern.isTickWritten (0));
}

TEST (PatternWrittenTicks, AnOutOfRangeTickIsNotWritten)
{
  Pattern pattern;
  pattern.resize (16);

  EXPECT_FALSE (pattern.isTickWritten (pattern.getNumTicks ()));
}
