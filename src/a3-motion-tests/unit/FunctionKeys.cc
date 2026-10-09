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

namespace
{
EndKey
at (PanelSide side, int row)
{
  return endKeyAt (side, row);
}

std::optional<FunctionKey>
shifted (PanelSide side, int row)
{
  return shiftedFunction (endKeyAt (side, row));
}
}

// The end columns as the maintainer laid them out: TAP, SHIFT and PLAY all
// across the top three rows of both sides, the six actions below them in the
// pads' reading order -- A1 A3 A5 on the left, A2 A4 A6 on the right.
TEST (FunctionKeys, TheEndColumnsAreTapShiftPlayAllAndTheSixActions)
{
  for (auto const side : { PanelSide::Left, PanelSide::Right })
    {
      EXPECT_EQ (at (side, 0), EndKey::Tap);
      EXPECT_EQ (at (side, 1), EndKey::Shift);
      EXPECT_EQ (at (side, 2), EndKey::PlayAll);
    }

  EXPECT_EQ (at (PanelSide::Left, 3), EndKey::Action1);
  EXPECT_EQ (at (PanelSide::Left, 4), EndKey::Action3);
  EXPECT_EQ (at (PanelSide::Left, 5), EndKey::Action5);
  EXPECT_EQ (at (PanelSide::Right, 3), EndKey::Action2);
  EXPECT_EQ (at (PanelSide::Right, 4), EndKey::Action4);
  EXPECT_EQ (at (PanelSide::Right, 5), EndKey::Action6);
}

// TAP, SHIFT and PLAY all are one key each with two places to press it; the
// action rows are six keys. Nine keys on twelve places, every one of them
// reachable.
TEST (FunctionKeys, NineKeysStandOnTwelvePlaces)
{
  std::set<EndKey> seen;
  for (int row = 0; row < numEndRows; ++row)
    for (auto const side : { PanelSide::Left, PanelSide::Right })
      seen.insert (at (side, row));

  EXPECT_EQ (seen.size (), static_cast<std::size_t> (numEndKeys));
  for (auto const key : allEndKeys)
    EXPECT_EQ (seen.count (key), 1u);
}

TEST (FunctionKeys, AnActionKeyNamesItsButton)
{
  EXPECT_EQ (endKeyActionButton (EndKey::Action1), 0);
  EXPECT_EQ (endKeyActionButton (EndKey::Action2), 1);
  EXPECT_EQ (endKeyActionButton (EndKey::Action6), 5);
  EXPECT_EQ (endKeyActionButton (EndKey::Tap), -1);
  EXPECT_EQ (endKeyActionButton (EndKey::Shift), -1);
  EXPECT_EQ (endKeyActionButton (EndKey::PlayAll), -1);
}

// TAP and SHIFT are functions of their own; PLAY all and the actions are the
// pads across every channel and carry no function key.
TEST (FunctionKeys, OnlyTapAndShiftAreFunctionsUnshifted)
{
  EXPECT_EQ (plainFunction (EndKey::Tap), FunctionKey::Tap);
  EXPECT_EQ (plainFunction (EndKey::Shift), FunctionKey::Shift);
  EXPECT_EQ (plainFunction (EndKey::PlayAll), std::nullopt);
  EXPECT_EQ (plainFunction (EndKey::Action1), std::nullopt);
}

// The four keys that lost their place, under SHIFT: TAP is the clock, PLAY all
// is REC, the row 3 keys are rec mode and the row 5 keys MENU. Row 4 stays
// free, and so does SHIFT itself.
TEST (FunctionKeys, TheShiftLayerHoldsTheFourKeysThatMoved)
{
  for (auto const side : { PanelSide::Left, PanelSide::Right })
    {
      EXPECT_EQ (shifted (side, 0), FunctionKey::ClockMode);
      EXPECT_EQ (shifted (side, 1), std::nullopt);
      EXPECT_EQ (shifted (side, 2), FunctionKey::Record);
      EXPECT_EQ (shifted (side, 3), FunctionKey::RecMode);
      EXPECT_EQ (shifted (side, 4), std::nullopt);
      EXPECT_EQ (shifted (side, 5), FunctionKey::Menu);
    }
}

// Every function is reachable from the end columns, plain or shifted.
TEST (FunctionKeys, EveryFunctionHasAKey)
{
  std::set<FunctionKey> reached;
  for (auto const key : allEndKeys)
    {
      if (auto const f = plainFunction (key))
        reached.insert (*f);
      if (auto const f = shiftedFunction (key))
        reached.insert (*f);
    }

  EXPECT_EQ (reached.size (), static_cast<std::size_t> (numFunctionKeys));
}

// The LEDs and the panel's keyboard light a key at its first place: TAP's,
// SHIFT's and PLAY all's on the left, an action's on its own side.
TEST (FunctionKeys, AKeyKnowsWhereItStands)
{
  EXPECT_EQ (firstPlaceOf (EndKey::Tap).side, PanelSide::Left);
  EXPECT_EQ (firstPlaceOf (EndKey::Tap).row, 0);
  EXPECT_EQ (firstPlaceOf (EndKey::PlayAll).row, 2);
  EXPECT_EQ (firstPlaceOf (EndKey::Action4).side, PanelSide::Right);
  EXPECT_EQ (firstPlaceOf (EndKey::Action4).row, 4);
  static_assert (endKeyAt (PanelSide::Right, 5) == EndKey::Action6);
}
