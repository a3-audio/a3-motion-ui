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

#include <set>

#include <a3-motion-ui/io/FunctionKeys.hh>

using namespace a3;

// The order the maintainer asked for, written down once. The panel is wired
// from it and the global strip is laid out from it, so a hand that has learned
// one has learned the other.
TEST (FunctionKeys, TheOrderIsTapClockRecRecmodeMenuShift)
{
  EXPECT_EQ (functionKeyOrder[0], FunctionKey::Tap);
  EXPECT_EQ (functionKeyOrder[1], FunctionKey::ClockMode);
  EXPECT_EQ (functionKeyOrder[2], FunctionKey::Record);
  EXPECT_EQ (functionKeyOrder[3], FunctionKey::RecMode);
  EXPECT_EQ (functionKeyOrder[4], FunctionKey::Menu);
  EXPECT_EQ (functionKeyOrder[5], FunctionKey::Shift);
}

// Every key exactly once. A key listed twice would take two places on the
// panel and leave one of the six with nothing on it; a key missing would be
// reachable on the screen and not under a hand.
TEST (FunctionKeys, EveryKeyAppearsExactlyOnce)
{
  std::set<FunctionKey> seen;
  for (auto key : functionKeyOrder)
    EXPECT_TRUE (seen.insert (key).second) << "listed twice";

  EXPECT_EQ (seen.size (), static_cast<std::size_t> (numFunctionKeys));

  for (auto key : { FunctionKey::Record, FunctionKey::Tap,
                    FunctionKey::ClockMode, FunctionKey::Menu,
                    FunctionKey::Shift, FunctionKey::RecMode })
    EXPECT_GE (functionKeyPosition (key), 0);
}

// Read at compile time, since both the panel's table and the strip's layout
// are built from it.
TEST (FunctionKeys, ThePositionsAreKnownAtCompileTime)
{
  static_assert (functionKeyPosition (FunctionKey::Tap) == 0);
  static_assert (functionKeyPosition (FunctionKey::Shift) == 5);
  SUCCEED ();
}
