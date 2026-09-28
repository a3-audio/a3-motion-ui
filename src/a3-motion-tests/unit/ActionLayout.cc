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

#include <a3-motion-ui/components/ActionLayout.hh>
#include <a3-motion-ui/components/ControllerLayout.hh>

using namespace a3;

namespace
{
constexpr float headerSize = 18.f;
}

// It has a whole page, so its controls have no excuse for being small. The
// clip bar's sections fight over a third of the bar each; this one fights with
// nobody.
TEST (ActionLayout, EveryControlIsWellOverAFingertip)
{
  for (int width : { 480, 640, 768, 1024 })
    for (int height : { 160, 250, 400 })
      {
        auto const l
            = layOutActionPage ({ 0, 0, width, height }, headerSize, 14.f, 1.f, {});

        for (size_t i = 0; i < l.controls.size (); ++i)
          {
            EXPECT_GE (l.controls[i].getWidth (), fingertipSize)
                << "control " << i << " at " << width << "x" << height;
            EXPECT_GE (l.controls[i].getHeight (), fingertipSize)
                << "control " << i << " at " << width << "x" << height;
          }
      }
}

// Three rows now: the accent, the cutoff, the resonance, each the same shape.
// Left to right within a row, and the columns line up so atk sits over atk --
// which is the whole reason the mode left the bottom row for the header.
TEST (ActionLayout, EachRowReadsLeftToRight)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, {});

  for (int row = 0; row < ActionLayout::numRows; ++row)
    for (size_t i = 1; i < 3; ++i)
      {
        auto const at = static_cast<size_t> (row * 3) + i;
        EXPECT_GE (l.controls[at].getX (), l.controls[at - 1].getRight ())
            << "control " << at << " overlaps its neighbour";
      }

  // Reading order down the page, and no row runs into the next.
  for (int row = 1; row < ActionLayout::numRows; ++row)
    EXPECT_LE (l.controls[static_cast<size_t> ((row - 1) * 3)].getBottom (),
               l.controls[static_cast<size_t> (row * 3)].getY ())
        << "row " << row << " runs into the one above it";

  // Same columns on every row: atk over atk, dec over dec, max over max.
  for (int row = 1; row < ActionLayout::numRows; ++row)
    for (size_t i = 0; i < 3; ++i)
      EXPECT_EQ (l.controls[i].getX (),
                 l.controls[static_cast<size_t> (row * 3) + i].getX ())
          << "column " << i << " does not line up on row " << row;

  EXPECT_LE (l.controls.back ().getRight (), 768);
}

// Every row says which envelope it is, in a column of its own that no knob
// stands in -- three rows of three unlabelled knobs would be nine numbers with
// nothing to tell them apart.
TEST (ActionLayout, EveryRowIsNamedBesideIt)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, {});

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      auto const &label = l.rowLabels[static_cast<size_t> (row)];
      EXPECT_FALSE (label.isEmpty ()) << "row " << row << " has no name";

      for (auto const &control : l.controls)
        EXPECT_FALSE (label.intersects (control))
            << "row " << row << "'s name is drawn over a knob";
    }
}



// Everything inside the page it was given.
TEST (ActionLayout, NothingEscapesThePage)
{
  auto const page = juce::Rectangle<int>{ 0, 0, 640, 200 };
  auto const l = layOutActionPage (page, headerSize, 14.f, 1.f, {});

  for (auto const &field : l.actionFields)
    EXPECT_TRUE (page.contains (field));
  EXPECT_TRUE (page.contains (l.actionListArea));
  EXPECT_TRUE (page.contains (l.actModeField));
  EXPECT_TRUE (page.contains (l.editButton));
  for (auto const &control : l.controls)
    EXPECT_TRUE (page.contains (control));
  for (auto const &label : l.rowLabels)
    EXPECT_TRUE (page.contains (label));
}

// ── The card, and lining up with the global strip ────────────────────────

// The nine knobs live in a card at the right, the way every other block of
// controls in the bar does -- and the space that frees on the left is where
// the script goes.
TEST (ActionLayout, TheKnobsStandInACardOnTheRight)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, {});

  ASSERT_FALSE (l.card.isEmpty ());
  for (auto const &control : l.controls)
    EXPECT_TRUE (l.card.contains (control)) << "a knob stands outside the card";
  for (auto const &label : l.rowLabels)
    EXPECT_TRUE (l.card.contains (label)) << "a row name stands outside the card";

  ASSERT_FALSE (l.actionListArea.isEmpty ());
  EXPECT_FALSE (l.actionListArea.intersects (l.card))
      << "the list runs into the knobs";
  EXPECT_LE (l.actionListArea.getRight (), l.card.getX ())
      << "the list belongs left of the card";
}

// Three rows of three: atk, dec, max across; 3d, freq, q down. The same shape
// as the global strip's channel grid, because it is the same kind of thing --
// a small block of knobs read by row and column rather than one at a time.
TEST (ActionLayout, TheKnobsAreAGridOfThreeByThree)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, {});

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      for (size_t i = 1; i < 3; ++i)
        {
          auto const at = static_cast<size_t> (row * 3) + i;
          EXPECT_GE (l.controls[at].getX (), l.controls[at - 1].getRight ())
              << "control " << at << " overlaps its neighbour";
        }

      // The row's name stands left of its knobs, in a gutter of its own.
      auto const &label = l.rowLabels[static_cast<size_t> (row)];
      EXPECT_LE (label.getRight (), l.controls[static_cast<size_t> (row * 3)].getX ());
    }

  for (int row = 1; row < ActionLayout::numRows; ++row)
    for (size_t i = 0; i < 3; ++i)
      EXPECT_EQ (l.controls[i].getX (),
                 l.controls[static_cast<size_t> (row * 3) + i].getX ())
          << "column " << i << " does not line up on row " << row;
}

// Handed the global strip's rows, the page puts its own on exactly those --
// so 3d, freq and q read straight across the bar, the page's knobs beside the
// strip's channels. Without a reference it lays itself out and says nothing.
TEST (ActionLayout, GivenTheGlobalRowsItSitsOnThem)
{
  juce::Rectangle<int> const reference{ 0, 80, 10, 120 }; // three rows of 40

  auto const l
      = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, reference);

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      auto const &band = l.rows[static_cast<size_t> (row)];
      EXPECT_EQ (band.getY (), 80 + row * 40) << "row " << row;
      EXPECT_EQ (band.getHeight (), 40) << "row " << row;
    }
}

// But never above the card's own name. Lining up with the strip is worth
// having; a knob drawn over the word that says what the knobs are is not.
TEST (ActionLayout, ItWillNotLineUpOverTheCardsName)
{
  juce::Rectangle<int> const wayUp{ 0, 0, 10, 120 };

  auto const l
      = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, wayUp);

  ASSERT_FALSE (l.cardCaption.isEmpty ());
  for (auto const &control : l.controls)
    EXPECT_GE (control.getY (), l.cardCaption.getBottom ());
}


// A reference that would push the rows off the page is ignored rather than
// obeyed -- and so is one whose rows are too short to land a finger on.
// Lining up is worth having and worth losing; a knob drawn past the bottom
// edge, or one too small to hit, is worth neither.
TEST (ActionLayout, AReferenceThatWouldNotFitIsIgnored)
{
  juce::Rectangle<int> const tooLow{ 0, 260, 10, 120 };

  auto const l
      = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, tooLow);

  for (auto const &control : l.controls)
    EXPECT_LE (control.getBottom (), 300) << "a knob was drawn off the page";
}

TEST (ActionLayout, ARowTooShortToHitIsIgnoredToo)
{
  juce::Rectangle<int> const cramped{ 0, 40, 10, 45 }; // three rows of 15

  auto const l
      = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f, cramped);

  for (auto const &control : l.controls)
    EXPECT_GE (control.getHeight (), fingertipSize)
        << "the page lined up with a row no finger could land on";
}

// ── The list the action field opens ──────────────────────────────────────



// Every row of it is a fingertip, whatever the page's size -- picking a
// script mid-set is a tap, and a row you have to aim at is a row you miss.
TEST (ActionLayout, EveryListRowIsAFingertip)
{
  for (int height : { 200, 300, 400 })
    {
      auto const l
          = layOutActionPage ({ 0, 0, 768, height }, headerSize, 14.f, 1.f, {});

      EXPECT_GE (l.actionListRowHeight, fingertipSize)
          << "at height " << height;
    }
}

// ── Save and cancel ──────────────────────────────────────────────────────

// The keys under the script and the room the text keeps are the ScriptPanel's
// since 2026-09-27: see unit/ScriptPanelLayout.cc
// (FourKeysAtTheFootAFingertipEach, ErrorsTakeTheirRoomOffTheText).

/** The list opens over the script field, its rows are a fingertip tall, and
 *  there are twenty-one things to choose from once the folder is full. Those
 *  three facts together mean it does not fit, which is the whole reason it
 *  has to scroll -- so the arithmetic that says how much of it is on screen
 *  is worth pinning down. */
TEST (ActionLayout, TheActionListShowsFewerRowsThanThereAreScripts)
{
  auto const layout = layOutActionPage ({ 0, 0, 768, 256 }, 14.f, 12.f, 40, {});

  auto const rows = actionListVisibleRows (layout);
  EXPECT_GE (rows, 1);
  EXPECT_LT (rows, 21) << "twenty scripts and 'no action' cannot all fit";
  EXPECT_LE (rows * layout.actionListRowHeight,
             layout.actionListArea.getHeight ())
      << "a row counted as visible must actually be inside the field";
}


// -- Six buttons, the list, the keys, the card (2026-09-28) -------------------
//
// Left to right: the six action buttons, three rows of two as on the panel;
// the list the chosen one is assigned from; a column of keys, ACT at its foot
// with EDIT and the mode above; and the card of nine knobs for the chosen
// button's feel.

namespace
{
ActionLayout
pageLayout ()
{
  return layOutActionPage ({ 0, 0, 576, 300 }, headerSize, 14.f, 1.f, {});
}
}

TEST (ActionLayout, TheSixButtonsStandThreeRowsOfTwoAsOnThePanel)
{
  auto const l = pageLayout ();
  for (size_t row = 0; row < 3; ++row)
    {
      auto const &left = l.actionFields[row * 2];
      auto const &right = l.actionFields[row * 2 + 1];
      EXPECT_EQ (left.getY (), right.getY ()) << row;
      EXPECT_LE (left.getRight (), right.getX ()) << row;
      if (row > 0)
        {
          EXPECT_GE (left.getY (), l.actionFields[(row - 1) * 2].getBottom ())
              << row;
          EXPECT_EQ (left.getX (), l.actionFields[0].getX ()) << row;
        }
    }
}

TEST (ActionLayout, EveryActionButtonIsAFingertip)
{
  for (int height : { 200, 256, 300 })
    {
      auto const l
          = layOutActionPage ({ 0, 0, 576, height }, headerSize, 14.f, 1.f, {});
      for (auto const &field : l.actionFields)
        {
          EXPECT_GE (field.getWidth (), fingertipSize) << height;
          EXPECT_GE (field.getHeight (), fingertipSize) << height;
        }
    }
}

TEST (ActionLayout, TheListStandsBetweenTheButtonsAndTheKeys)
{
  auto const l = pageLayout ();
  ASSERT_FALSE (l.actionListArea.isEmpty ());
  for (auto const &field : l.actionFields)
    EXPECT_LE (field.getRight (), l.actionListArea.getX ());
  EXPECT_LE (l.actionListArea.getRight (), l.editButton.getX ());
  EXPECT_GE (actionListVisibleRows (l), 3);
}

// EDIT at the top of the column and the mode under it (2026-09-28). There
// is no ACT key on the page any more: the six fields fire their own actions.
TEST (ActionLayout, TheKeysStandInAColumnEditThenMode)
{
  auto const l = pageLayout ();
  for (auto const key : { l.editButton, l.actModeField })
    {
      ASSERT_FALSE (key.isEmpty ());
      EXPECT_EQ (key.getX (), l.editButton.getX ());
      EXPECT_GE (key.getWidth (), fingertipSize);
      EXPECT_GE (key.getHeight (), fingertipSize);
      EXPECT_LE (key.getRight (), l.card.getX ());
    }
  EXPECT_EQ (l.editButton.getY (), l.actionListArea.getY ());
  EXPECT_LE (l.editButton.getBottom (), l.actModeField.getY ());
}

TEST (ActionLayout, TheCardHoldsOnlyTheKnobs)
{
  auto const l = pageLayout ();
  for (auto const &control : l.controls)
    EXPECT_TRUE (l.card.contains (control));
  EXPECT_FALSE (l.card.intersects (l.actModeField));
}
