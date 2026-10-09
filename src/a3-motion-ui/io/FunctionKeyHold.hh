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
#include <cstddef>
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

/** Whether each key is down, from all the places it can be pressed together.
 *
 *  One key often has two places: TAP stands at both ends of the panel, and
 *  the PADS page carries the panel's keys too (2026-09-28). The answer is the
 *  same every time -- a key is down while *any* place holds it. `set()` says
 *  when that combined state actually changed, so what a key means is handled
 *  once per press and once per release, whichever hand it came from, and
 *  letting go of one place while another still holds is no release.
 */
template <typename Key, int numKeys, typename Place>
class KeyHold
{
public:
  /** Record where a key now stands. Returns the key's new combined state if
   *  it changed, nothing otherwise. */
  std::optional<bool>
  set (Key key, Place place, bool down)
  {
    auto &places = _down[slot (key)];
    auto const before = places[0] || places[1];
    places[static_cast<std::size_t> (place)] = down;
    auto const after = places[0] || places[1];

    if (after == before)
      return std::nullopt;
    return after;
  }

  bool
  isDown (Key key) const
  {
    auto const &places = _down[slot (key)];
    return places[0] || places[1];
  }

private:
  static std::size_t
  slot (Key key)
  {
    return static_cast<std::size_t> (key);
  }

  std::array<std::array<bool, 2>, static_cast<std::size_t> (numKeys)> _down{};
};

/** The panel's two end columns as one set of keys. */
using EndColumnHold = KeyHold<EndKey, numEndKeys, PanelSide>;

/** The panel and the PADS page as one set of keys. */
using EndKeyHold = KeyHold<EndKey, numEndKeys, KeySource>;

}
