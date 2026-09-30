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

#include <a3-motion-ui/components/BarKeyboardComponent.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>
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
  EXPECT_EQ (rowText (KeyboardPage::Symbols, 4), "{}[]<>\\|!~");
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

// Every character a name or a script needs is on one of the two pages. "~"
// takes the place "?" was designed for: the shipped scripts use it 2122
// times (SuperCollider's environment variables), "?" not once. No umlauts:
// QWERTY, and the shipped library has none.
TEST (PanelKeyboard, EveryCharacterANameOrScriptNeedsIsReachable)
{
  std::set<juce::juce_wchar> reachable;
  for (auto page : { KeyboardPage::Letters, KeyboardPage::Symbols })
    for (auto const &cell : panelCells ())
      {
        auto const key = panelKeyAt (page, cell);
        if (key.action != KeyAction::Character)
          continue;
        reachable.insert (key.character);
        reachable.insert (upperCaseOf (key.character));
      }

  juce::String const needed
      = juce::String ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                      "0123456789 -_.")
        + "/\":,~;=\\{}'()[]+*<>|";
  for (auto character : needed)
    EXPECT_TRUE (reachable.count (character) > 0)
        << "missing: " << juce::String::charToString (character);
}

// -- On the screen -----------------------------------------------------------

namespace
{
juce::Rectangle<int> const barArea{ 0, 0, 576, 300 };
}

// A key on the screen stands where PADS has the same cell: the page is the
// panel, and so is the keyboard.
TEST (PanelKeyboard, TheKeysStandWherePadsHasTheirCells)
{
  auto const keys = layOutPanelKeyboard (barArea, KeyboardPage::Letters);
  auto const pads = layOutController (barArea, 0.f, 0);

  auto const capOf = [&] (PanelCell cell) {
    for (auto const &key : keys)
      if (key.cell == cell)
        return key.bounds;
    return juce::Rectangle<int>{};
  };
  EXPECT_EQ (capOf ({ 2, 1 }), pads.pads[0][0]);
  EXPECT_EQ (capOf ({ 5, 8 }), pads.pads[3][7]);
  EXPECT_EQ (capOf ({ 0, 9 }), pads.keys[0]) << "BKSP where TAP is";
  EXPECT_EQ (capOf ({ 2, 0 }), pads.scenes[0][0]);
}

// The four space cells are one bar on the screen -- on the panel they stay
// four keys that all type a space.
TEST (PanelKeyboard, TheSpacesAreOneBarOnTheScreen)
{
  auto const keys = layOutPanelKeyboard (barArea, KeyboardPage::Letters);
  EXPECT_EQ (keys.size (), 41u);

  int spaces = 0;
  juce::Rectangle<int> bar;
  for (auto const &key : keys)
    if (key.def.action == KeyAction::Character && key.def.character == ' ')
      {
        ++spaces;
        bar = key.bounds;
      }
  ASSERT_EQ (spaces, 1);
  EXPECT_EQ (bar.getX (), panelCellBounds (barArea, { 5, 3 }).getX ());
  EXPECT_EQ (bar.getRight (), panelCellBounds (barArea, { 5, 6 }).getRight ());
}

TEST (PanelKeyboard, KeyAtFindsTheKeyUnderAPoint)
{
  auto const keys = layOutPanelKeyboard (barArea, KeyboardPage::Symbols);
  for (size_t i = 0; i < keys.size (); ++i)
    EXPECT_EQ (keyAt (keys, keys[i].bounds.getCentre ()), static_cast<int> (i));
  EXPECT_EQ (keyAt (keys, { -100, -100 }), -1);
}

// A page test must paint (the ACTION page crashed on its first frame with a
// green suite, 2026-09-28).
TEST (PanelKeyboard, TheKeyboardPaintsBothPages)
{
  BarKeyboardComponent keyboard;
  keyboard.setBounds (barArea);
  keyboard.setMetrics ({ 40, 14.f, 12.f });
  auto const letters = keyboard.createComponentSnapshot (barArea);
  EXPECT_EQ (letters.getWidth (), barArea.getWidth ());

  keyboard.pressPanelCell ({ 1, 0 }, true);   // 123
  keyboard.pressPanelCell ({ 1, 0 }, false);
  auto const symbols = keyboard.createComponentSnapshot (barArea);
  EXPECT_EQ (symbols.getWidth (), barArea.getWidth ());
}
