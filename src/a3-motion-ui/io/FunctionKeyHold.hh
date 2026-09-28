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
#include <optional>

#include <a3-motion-ui/io/FunctionKeys.hh>

namespace a3
{

/** Where a function key was pressed. */
enum class KeySource
{
  Panel,
  Screen,
};

/** Whether each function key is down, from the panel and the screen together.
 *
 *  The PADS page carries the panel's keys (2026-09-28), so a key can now be
 *  held in two places -- the same situation as the panel's own two end
 *  columns, and the same answer: a key is down while *either* place holds it.
 *  `set()` says when that combined state actually changed, so what a key
 *  means is handled once per press and once per release, whichever hand it
 *  came from, and letting go of one place while the other still holds is no
 *  release.
 */
class FunctionKeyHold
{
public:
  /** Record where a key now stands. Returns the key's new combined state if
   *  it changed, nothing otherwise. */
  std::optional<bool>
  set (FunctionKey key, KeySource source, bool down)
  {
    auto &sides = _down[slot (key)];
    auto const before = sides[0] || sides[1];
    sides[static_cast<std::size_t> (source)] = down;
    auto const after = sides[0] || sides[1];

    if (after == before)
      return std::nullopt;
    return after;
  }

  bool
  isDown (FunctionKey key) const
  {
    auto const &sides = _down[slot (key)];
    return sides[0] || sides[1];
  }

private:
  static std::size_t
  slot (FunctionKey key)
  {
    return static_cast<std::size_t> (functionKeyPosition (key));
  }

  /** [key's position in functionKeyOrder][KeySource] */
  std::array<std::array<bool, 2>, numFunctionKeys> _down{};
};

}
