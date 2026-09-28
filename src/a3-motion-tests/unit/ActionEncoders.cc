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

// ACTION on the panel (maintainer, 2026-09-28): enc 1 chooses A1..A6, enc 2
// walks the list with a highlight a press assigns, enc 3 rings a key of the
// key column and a press presses it, enc 4 switches AUDIO/MOTION. enc 5..8
// turn the four values of the tile's marked row; a press marks the next row.

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/components/ActionComponent.hh>
#include <a3-motion-ui/components/EncoderMap.hh>

#include <optional>
#include <vector>

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
  EXPECT_EQ (onAction (0, top), EncoderTarget::Kind::ActionButton);
  EXPECT_EQ (onAction (1, top), EncoderTarget::Kind::ActionList);
  EXPECT_EQ (onAction (2, top), EncoderTarget::Kind::ActionKey);
  EXPECT_EQ (onAction (3, top), EncoderTarget::Kind::ActionTile);
}

TEST (ActionEncoders, TheLowerFourTurnTheMarkedRowsValues)
{
  for (int column = 0; column < 4; ++column)
    {
      auto const t = encoderTarget (BarPage::Action, column, bottom, false,
                                    false);
      EXPECT_EQ (t.kind, EncoderTarget::Kind::ActionValue);
      EXPECT_EQ (t.sub, column);
    }
}

TEST (ActionEncoders, ShiftKeepsTheChannelsFreqAndQ)
{
  for (int column = 0; column < 4; ++column)
    for (int row : { top, bottom })
      EXPECT_EQ (onAction (column, row, true),
                 EncoderTarget::Kind::ColumnChannelPot);
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

// ── enc 3: the key column ───────────────────────────────────────────────

TEST (ActionEncoders, TheRingWalksEditHoldThenAndStopsAtTheEnds)
{
  Page p;
  EXPECT_EQ (p.page.moveKeyRing (1), ActionKey::Mode);
  EXPECT_EQ (p.page.moveKeyRing (1), ActionKey::After);
  EXPECT_EQ (p.page.moveKeyRing (5), ActionKey::After);
  EXPECT_EQ (p.page.moveKeyRing (-5), ActionKey::Edit);
}

TEST (ActionEncoders, APressDoesWhatATapOnTheRingedKeyDoes)
{
  Page p;
  int edits = 0;
  std::vector<int> tapped;
  int afterSteps = 0;
  p.page.onEditPressed = [&edits] { ++edits; };
  p.page.onControlTapped = [&tapped] (int control) { tapped.push_back (control); };
  p.page.onAfterStepped = [&afterSteps] (int increment) { afterSteps += increment; };

  p.page.pressKeyRing ();
  EXPECT_EQ (edits, 1) << "the ring starts on EDIT";

  p.page.moveKeyRing (1);
  p.page.pressKeyRing ();
  ASSERT_EQ (tapped.size (), 1u);
  EXPECT_EQ (tapped[0], ActionComponent::ActMode);

  p.page.moveKeyRing (1);
  p.page.pressKeyRing ();
  EXPECT_EQ (afterSteps, 1);
}

// ── enc 4: the tiles, now tabs on the card ──────────────────────────────

// Where the tabs stand is ActionLayout.AudioAndMotionAreTabsOnTopOfTheCard.

TEST (ActionEncoders, SwitchingTheTileGoesBackAndForth)
{
  Page p;
  p.page.switchTile ();
  EXPECT_EQ (p.page.tile (), ActionTile::Motion);
  p.page.switchTile ();
  EXPECT_EQ (p.page.tile (), ActionTile::Audio);
}

// ── enc 5..8: the marked row ────────────────────────────────────────────

TEST (ActionEncoders, OnAudioTheFirstThreeTurnTheMarkedRowsKnobs)
{
  Page p;
  std::vector<std::pair<int, int> > dragged;
  p.page.onControlDragged = [&dragged] (int control, int increment) {
    dragged.emplace_back (control, increment);
  };

  p.page.turnMarkedValue (1, 2);
  p.page.stepValueRow ();
  p.page.turnMarkedValue (0, -1);
  p.page.turnMarkedValue (3, 1); // AUDIO has three across: nothing

  ASSERT_EQ (dragged.size (), 2u);
  EXPECT_EQ (dragged[0], std::make_pair (int (ActionComponent::Decay), 2));
  EXPECT_EQ (dragged[1], std::make_pair (int (ActionComponent::FreqAttack), -1));
}

TEST (ActionEncoders, APressMarksTheNextRowAndComesRound)
{
  Page p;
  EXPECT_EQ (p.page.markedValueRow (), 0);
  p.page.stepValueRow ();
  p.page.stepValueRow ();
  EXPECT_EQ (p.page.markedValueRow (), 2);
  p.page.stepValueRow ();
  EXPECT_EQ (p.page.markedValueRow (), 0) << "AUDIO has three rows";
}

TEST (ActionEncoders, OnMotionTheFourTurnTheMarkedRow)
{
  Page p;
  p.page.setMotionTile ({}, true, 4.f);
  p.page.switchTile ();
  std::vector<MotionParam> set;
  p.page.onMotionSet = [&set] (MotionParam param, float) { set.push_back (param); };

  p.page.turnMarkedValue (0, 1);
  p.page.turnMarkedValue (3, 1);
  for (int row = 1; row <= 4; ++row)
    p.page.stepValueRow ();
  p.page.turnMarkedValue (0, 1); // the last row: speed, dir, end
  p.page.turnMarkedValue (3, 1); // nothing stands there

  ASSERT_EQ (set.size (), 3u);
  EXPECT_EQ (set[0], motionParamOrder[0]);
  EXPECT_EQ (set[1], motionParamOrder[3]);
  EXPECT_EQ (set[2], motionParamOrder[16]);

  p.page.stepValueRow ();
  EXPECT_EQ (p.page.markedValueRow (), 0) << "MOTION has five rows";
}

TEST (ActionEncoders, ATurnOnAMotionKnobMovesItsValue)
{
  Page p;
  std::array<MotionShown, numMotionParams> shown{};
  p.page.setMotionTile (shown, true, 4.f);
  p.page.switchTile ();
  std::optional<float> spin;
  p.page.onMotionSet = [&spin] (MotionParam param, float value) {
    if (param == MotionParam::Spin)
      spin = value;
  };
  p.page.turnMarkedValue (0, 1);
  ASSERT_TRUE (spin.has_value ());
  EXPECT_GT (*spin, 0.f);
}

TEST (ActionEncoders, AnotherTileStartsOnItsFirstRow)
{
  Page p;
  p.page.stepValueRow ();
  p.page.switchTile ();
  EXPECT_EQ (p.page.markedValueRow (), 0);
}
