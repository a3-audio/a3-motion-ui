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

#pragma once

#include <array>

namespace a3
{

/** The device's function keys, and the order they are in.
 *
 *  The panel carries six of them **twice**: a vertical column at each end,
 *  mirrored so either hand can reach them without the other letting go of
 *  what it is doing. The two columns are one set of keys, not twelve — a key
 *  is down while *either* side of it is down.
 *
 *  The screen shows the same six in the global strip, as two columns of three
 *  filled top-left to bottom-right. That is the same list read a different
 *  way, so it is the same list: `functionKeyOrder` below is what the panel is
 *  wired to *and* what the strip is laid out from, because a hand that learns
 *  one has learned the other, and two tables would eventually disagree.
 */
enum class FunctionKey
{
  Record,
  Tap,
  ClockMode,
  Menu,
  Shift,
  RecMode,
};

constexpr int numFunctionKeys = 6;

/** Top to bottom on the panel; top-left to bottom-right on the screen. */
constexpr std::array<FunctionKey, numFunctionKeys> functionKeyOrder{
  FunctionKey::Tap,     FunctionKey::ClockMode, FunctionKey::Record,
  FunctionKey::RecMode, FunctionKey::Menu,      FunctionKey::Shift,
};

/** Where a key sits in that order, or -1 for one that is not in it. */
constexpr int
functionKeyPosition (FunctionKey key)
{
  for (int i = 0; i < numFunctionKeys; ++i)
    if (functionKeyOrder[static_cast<std::size_t> (i)] == key)
      return i;

  return -1;
}

}
