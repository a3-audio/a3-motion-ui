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

// ACTION on the panel (2026-09-28): the four upper encoders stand under the
// page's four columns -- the list, the six buttons, the mode, the "then" --
// and the list is walked with a highlight a press assigns, so turning past
// forty scripts mid-set never puts one on a button by accident.

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/ActionComponent.hh>
#include <a3-motion-ui/components/EncoderMap.hh>

using namespace a3;

namespace
{
constexpr int top = 0;
constexpr int bottom = 1;

EncoderTarget::Kind
onAction (int column, int row, bool shift = false)
{
  return encoderTarget (BarPage::Action, column, row, false, shift).kind;
}

struct Page
{
  ActionComponent page;
  Page ()
  {
    page.setVisible (true);
    page.setBounds (0, 0, 560, 268);
    page.setActionChoices ({ "", "Lift Up", "Move Spin", "Width Open" });
    page.setActionName ("Lift Up");
  }
};
}

TEST (ActionEncoders, TheUpperFourStandUnderThePagesColumns)
{
  EXPECT_EQ (onAction (0, top), EncoderTarget::Kind::ActionList);
  EXPECT_EQ (onAction (1, top), EncoderTarget::Kind::ActionButton);
  EXPECT_EQ (onAction (2, top), EncoderTarget::Kind::ActionMode);
  EXPECT_EQ (onAction (3, top), EncoderTarget::Kind::ActionAfter);
}

TEST (ActionEncoders, TheLowerFourAndShiftKeepTheChannelsFreqAndQ)
{
  for (int column = 0; column < 4; ++column)
    {
      EXPECT_EQ (onAction (column, bottom),
                 EncoderTarget::Kind::ColumnChannelPot);
      EXPECT_EQ (onAction (column, top, true),
                 EncoderTarget::Kind::ColumnChannelPot);
    }
}

TEST (ActionEncoders, TheHighlightStartsOnTheAssignedScript)
{
  Page p;
  EXPECT_EQ (p.page.moveListCursor (1), "Move Spin");
  EXPECT_EQ (p.page.moveListCursor (1), "Width Open");
}

TEST (ActionEncoders, TheHighlightStopsAtTheEnds)
{
  Page p;
  EXPECT_EQ (p.page.moveListCursor (10), "Width Open");
  EXPECT_EQ (p.page.moveListCursor (-10), "") << "no action is the first row";
}

TEST (ActionEncoders, TurningAssignsNothing)
{
  Page p;
  int assigned = 0;
  p.page.onActionChosen = [&assigned] (juce::String const &) { ++assigned; };
  p.page.moveListCursor (2);
  EXPECT_EQ (assigned, 0);
}

TEST (ActionEncoders, APressAssignsTheHighlightedScript)
{
  Page p;
  juce::String assigned = "untouched";
  p.page.onActionChosen
      = [&assigned] (juce::String const &name) { assigned = name; };
  p.page.moveListCursor (1);
  p.page.chooseListCursor ();
  EXPECT_EQ (assigned, "Move Spin");
}

TEST (ActionEncoders, AnotherButtonStartsTheHighlightOnItsOwnScript)
{
  Page p;
  p.page.moveListCursor (2);
  p.page.setActionName ("Move Spin");
  EXPECT_EQ (p.page.moveListCursor (-1), "Lift Up");
}

// The page is told the name on every update: that must not throw the walk
// back to where it started, or it never gets past one row.
TEST (ActionEncoders, BeingToldTheSameNameAgainKeepsTheWalk)
{
  Page p;
  p.page.moveListCursor (1);
  p.page.setActionName ("Lift Up");
  EXPECT_EQ (p.page.moveListCursor (1), "Width Open");
}

TEST (ActionEncoders, ChoosingAnotherButtonStartsTheWalkAgain)
{
  Page p;
  std::array<juce::String, 6> names{ "Lift Up", "Lift Up" };
  p.page.setActionButtons (names, 0);
  p.page.moveListCursor (2);
  p.page.setActionButtons (names, 1);
  EXPECT_EQ (p.page.moveListCursor (1), "Move Spin");
}
