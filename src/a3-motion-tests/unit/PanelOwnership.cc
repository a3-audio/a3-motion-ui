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

#include <a3-motion-ui/io/PanelButtonCells.hh>
#include <a3-motion-ui/io/PanelOwnership.hh>

#include <set>

using namespace a3;

// -- Which key is where ------------------------------------------------------

// The firmware numbers its 44 buttons in wiring order; each one's place on
// the panel is its "RC" label (InputOutputAdapterV3.hh). Every cell the
// panel has, exactly once.
TEST (PanelButtonCells, TheFortyFourButtonsAreThePanelsFortyFourCells)
{
  std::set<std::pair<int, int>> seen;
  for (int button = 0; button < numPanelButtons; ++button)
    {
      auto const cell = panelCellOfButton (button);
      EXPECT_TRUE (isPanelCell (cell)) << button;
      seen.insert ({ cell.row, cell.col });
    }
  EXPECT_EQ (seen.size (), panelCells ().size ());
}

TEST (PanelButtonCells, SomeButtonsByTheirLabels)
{
  EXPECT_EQ (panelCellOfButton (0), (PanelCell{ 4, 0 }));   // "40"
  EXPECT_EQ (panelCellOfButton (4), (PanelCell{ 2, 1 }));   // "21", ch0 pad0
  EXPECT_EQ (panelCellOfButton (35), (PanelCell{ 5, 8 }));  // "58", ch3 pad7
  EXPECT_EQ (panelCellOfButton (42), (PanelCell{ 0, 9 }));  // "09"
}

// -- Who the panel belongs to ------------------------------------------------

TEST (PanelOwnership, WithTheKeyboardDownEverythingGoesToThePanel)
{
  PanelOwnership owner;
  EXPECT_EQ (owner.route (4, true), PanelRoute::Panel);
  EXPECT_EQ (owner.route (4, false), PanelRoute::Panel);
}

TEST (PanelOwnership, WithTheKeyboardUpAPressAndItsReleaseType)
{
  PanelOwnership owner;
  owner.setOwnedByKeyboard (true);
  EXPECT_EQ (owner.route (4, true), PanelRoute::Keyboard);
  EXPECT_EQ (owner.route (4, false), PanelRoute::Keyboard);
}

// ESC sits on TAP. Pressed, it puts the keyboard away -- and the release that
// follows must not reach TAP, or it would set a downbeat.
TEST (PanelOwnership, AKeyThatHidTheKeyboardKeepsItsReleaseToo)
{
  PanelOwnership owner;
  owner.setOwnedByKeyboard (true);
  EXPECT_EQ (owner.route (40, true), PanelRoute::Keyboard);
  owner.setOwnedByKeyboard (false);
  EXPECT_EQ (owner.route (40, false), PanelRoute::Keyboard);
  // And the next press is the panel's again.
  EXPECT_EQ (owner.route (40, true), PanelRoute::Panel);
}

// A hold that began before the keyboard came up ends where it began: a pad
// held for an action lets go of it.
TEST (PanelOwnership, AHoldFromBeforeTheKeyboardIsReleasedOnThePanel)
{
  PanelOwnership owner;
  EXPECT_EQ (owner.route (9, true), PanelRoute::Panel);
  owner.setOwnedByKeyboard (true);
  EXPECT_EQ (owner.route (9, false), PanelRoute::Panel);
  EXPECT_EQ (owner.route (9, true), PanelRoute::Keyboard);
}

// A channel's pad `p` stands where the panel has it: two columns of four per
// channel, from column 1 -- the same cells the firmware's labels say.
TEST (PanelButtonCells, APadsCellIsItsLabel)
{
  EXPECT_EQ (panelCellOfPad (0, 0), panelCellOfButton (4));   // "21"
  EXPECT_EQ (panelCellOfPad (0, 3), panelCellOfButton (5));   // "51"
  EXPECT_EQ (panelCellOfPad (0, 6), panelCellOfButton (8));   // "42"
  EXPECT_EQ (panelCellOfPad (3, 7), panelCellOfButton (35));  // "58"
  EXPECT_EQ (panelCellOfPad (2, 4), panelCellOfButton (26));  // "26"
}
