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

#include <JuceHeader.h>

#include <a3-motion-ui/components/PanelKeyboard.hh>

#include <set>

using namespace a3;

namespace
{
/** A page's row `row` as a string of the characters its keys type, a key
 *  that types none written as '#'. */
std::string
rowText (KeyboardPage page, int row)
{
  std::string out;
  for (int col = 0; col < panelColumns; ++col)
    {
      PanelCell const cell{ row, col };
      if (!isPanelCell (cell))
        continue;
      auto const key = panelKeyAt (page, cell);
      out += key.action == KeyAction::Character
                 ? static_cast<char> (key.character)
                 : '#';
    }
  return out;
}
}

// The panel has 44 keys: its two end columns six rows each, the pads' eight
// columns in rows 2-5 between them. The keyboard is those 44, key for key.
TEST (PanelKeyboard, ItHasThePanelsFortyFourKeys)
{
  auto const &cells = panelCells ();
  EXPECT_EQ (cells.size (), 44u);

  std::set<std::pair<int, int>> distinct;
  for (auto const &cell : cells)
    {
      EXPECT_TRUE (isPanelCell (cell));
      distinct.insert ({ cell.row, cell.col });
    }
  EXPECT_EQ (distinct.size (), 44u);

  EXPECT_TRUE (isPanelCell ({ 0, 0 }));
  EXPECT_TRUE (isPanelCell ({ 1, 9 }));
  EXPECT_FALSE (isPanelCell ({ 0, 1 })) << "over the pads, the panel has pots";
  EXPECT_FALSE (isPanelCell ({ 1, 8 }));
  EXPECT_TRUE (isPanelCell ({ 5, 4 }));
}

// QWERTY (asked for on 2026-09-30), a row of ten where the panel has ten.
TEST (PanelKeyboard, TheLettersAreQwerty)
{
  EXPECT_EQ (rowText (KeyboardPage::Letters, 2), "qwertyuiop");
  EXPECT_EQ (rowText (KeyboardPage::Letters, 3), "asdfghjkl-");
  EXPECT_EQ (rowText (KeyboardPage::Letters, 4), "zxcvbnm,./");
}

TEST (PanelKeyboard, TheCornersAreEscapePageBackspaceAndEnter)
{
  EXPECT_EQ (panelKeyAt (KeyboardPage::Letters, { 0, 0 }).action,
             KeyAction::Escape);
  EXPECT_EQ (panelKeyAt (KeyboardPage::Letters, { 1, 0 }).action,
             KeyAction::Page);
  EXPECT_EQ (panelKeyAt (KeyboardPage::Letters, { 0, 9 }).action,
             KeyAction::Backspace);
  EXPECT_EQ (panelKeyAt (KeyboardPage::Letters, { 1, 9 }).action,
             KeyAction::Enter);
}

TEST (PanelKeyboard, TheBottomRowIsShiftCursorSpaceQuotesAndHide)
{
  auto const at = [] (int col) {
    return panelKeyAt (KeyboardPage::Letters, { 5, col });
  };
  EXPECT_EQ (at (0).action, KeyAction::Shift);
  EXPECT_EQ (at (1).action, KeyAction::Left);
  EXPECT_EQ (at (2).action, KeyAction::Right);
  for (int col = 3; col <= 6; ++col)
    {
      EXPECT_EQ (at (col).action, KeyAction::Character) << col;
      EXPECT_EQ (at (col).character, ' ') << col;
    }
  EXPECT_EQ (at (7).character, '\'');
  EXPECT_EQ (at (8).character, '"');
  EXPECT_EQ (at (9).action, KeyAction::Hide);
}

TEST (PanelKeyboard, TheSymbolsPageHoldsDigitsAndScriptCharacters)
{
  EXPECT_EQ (rowText (KeyboardPage::Symbols, 2), "1234567890");
  EXPECT_EQ (rowText (KeyboardPage::Symbols, 3), "-/:;()=+_*");
  EXPECT_EQ (rowText (KeyboardPage::Symbols, 4), "{}[]<>\\|!?");
  // The rest stands where it stands on the letters page.
  for (auto const row : { 0, 1, 5 })
    EXPECT_EQ (rowText (KeyboardPage::Symbols, row),
               rowText (KeyboardPage::Letters, row))
        << row;
}

// A cell the panel does not have is no key.
TEST (PanelKeyboard, ACellOffThePanelIsNoKey)
{
  EXPECT_EQ (panelKeyAt (KeyboardPage::Letters, { 0, 4 }).action,
             KeyAction::None);
  EXPECT_EQ (panelKeyAt (KeyboardPage::Letters, { 6, 0 }).action,
             KeyAction::None);
}
