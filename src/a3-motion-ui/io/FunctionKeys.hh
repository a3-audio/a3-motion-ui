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

namespace a3
{

/** What the device does with a function key, wherever that key stands. */
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

/** Every function once, for walking them. The order means nothing: where a
 *  function is reached is endKeyAt and shiftedFunctionAtRow below. */
constexpr std::array<FunctionKey, numFunctionKeys> allFunctionKeys{
  FunctionKey::Tap,     FunctionKey::ClockMode, FunctionKey::Record,
  FunctionKey::RecMode, FunctionKey::Menu,      FunctionKey::Shift,
};

/** Which end of the panel a key stands at: col0 or col9. */
enum class PanelSide
{
  Left,
  Right,
};

constexpr int numEndRows = 6;

/** A key in the panel's two end columns, by what it is without SHIFT.
 *
 *  TAP, SHIFT and PLAY all stand on both sides and are one key each: down
 *  while either side is down, so either hand reaches them without the other
 *  letting go. The action keys are six: A1 A3 A5 down the left, A2 A4 A6 down
 *  the right, the pads' own reading order. */
enum class EndKey
{
  Tap,
  Shift,
  PlayAll,
  Action1,
  Action2,
  Action3,
  Action4,
  Action5,
  Action6,
};

constexpr int numEndKeys = 9;

constexpr std::array<EndKey, numEndKeys> allEndKeys{
  EndKey::Tap,     EndKey::Shift,   EndKey::PlayAll,
  EndKey::Action1, EndKey::Action2, EndKey::Action3,
  EndKey::Action4, EndKey::Action5, EndKey::Action6,
};

/** **What stands where** in the end columns, `[row][side]`, row 0 at the top.
 *  The one table: the panel is read from it, its LEDs are written from it and
 *  the PADS page is laid out from it. */
constexpr std::array<std::array<EndKey, 2>, numEndRows> endKeyTable{ {
    { EndKey::Tap, EndKey::Tap },
    { EndKey::Shift, EndKey::Shift },
    { EndKey::PlayAll, EndKey::PlayAll },
    { EndKey::Action1, EndKey::Action2 },
    { EndKey::Action3, EndKey::Action4 },
    { EndKey::Action5, EndKey::Action6 },
} };

/** **What SHIFT makes of a row.** The four functions that have no key of
 *  their own are here: SHIFT+TAP the clock, SHIFT+PLAY all REC, SHIFT on
 *  row 3 rec mode, SHIFT on row 5 MENU. Both sides of a row shift alike, so
 *  either hand finds them; row 4 is free. */
constexpr std::array<std::optional<FunctionKey>, numEndRows>
    shiftedFunctionAtRow{
      FunctionKey::ClockMode, std::nullopt,      FunctionKey::Record,
      FunctionKey::RecMode,   std::nullopt,      FunctionKey::Menu,
    };

constexpr EndKey
endKeyAt (PanelSide side, int row)
{
  return endKeyTable[static_cast<std::size_t> (row)]
                    [side == PanelSide::Left ? 0u : 1u];
}

struct EndPlace
{
  PanelSide side;
  int row;
};

/** Where a key stands first, reading rows top down and the left side first. */
constexpr EndPlace
firstPlaceOf (EndKey key)
{
  for (int row = 0; row < numEndRows; ++row)
    for (auto const side : { PanelSide::Left, PanelSide::Right })
      if (endKeyAt (side, row) == key)
        return { side, row };
  return { PanelSide::Left, 0 };
}

/** The action button an action key fires on every channel (0..5), or -1. */
constexpr int
endKeyActionButton (EndKey key)
{
  switch (key)
    {
    case EndKey::Action1: return 0;
    case EndKey::Action2: return 1;
    case EndKey::Action3: return 2;
    case EndKey::Action4: return 3;
    case EndKey::Action5: return 4;
    case EndKey::Action6: return 5;
    case EndKey::Tap:
    case EndKey::Shift:
    case EndKey::PlayAll:
      return -1;
    }
  return -1;
}

/** The function a key is without SHIFT. PLAY all and the actions are pads
 *  across every channel, not functions. */
constexpr std::optional<FunctionKey>
plainFunction (EndKey key)
{
  switch (key)
    {
    case EndKey::Tap:   return FunctionKey::Tap;
    case EndKey::Shift: return FunctionKey::Shift;
    case EndKey::PlayAll:
    case EndKey::Action1:
    case EndKey::Action2:
    case EndKey::Action3:
    case EndKey::Action4:
    case EndKey::Action5:
    case EndKey::Action6:
      return std::nullopt;
    }
  return std::nullopt;
}

/** The function a key is with SHIFT held, from its row. SHIFT itself has
 *  none: it is the modifier. */
constexpr std::optional<FunctionKey>
shiftedFunction (EndKey key)
{
  if (key == EndKey::Shift)
    return std::nullopt;
  return shiftedFunctionAtRow[static_cast<std::size_t> (firstPlaceOf (key).row)];
}

}
