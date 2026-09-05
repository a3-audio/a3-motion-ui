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
            = layOutActionPage ({ 0, 0, width, height }, headerSize, 14.f, 1.f);

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
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f);

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
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f);

  for (int row = 0; row < ActionLayout::numRows; ++row)
    {
      auto const &label = l.rowLabels[static_cast<size_t> (row)];
      EXPECT_FALSE (label.isEmpty ()) << "row " << row << " has no name";

      for (auto const &control : l.controls)
        EXPECT_FALSE (label.intersects (control))
            << "row " << row << "'s name is drawn over a knob";
    }
}

// The mode stands beside the action's name, not at the end of a row: it says
// what a press does to all three envelopes, so it belongs to none of them --
// and what fires and how then sit on one line, which is one glance.
TEST (ActionLayout, TheModeStandsBesideTheActionsName)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f);

  ASSERT_FALSE (l.actModeField.isEmpty ());
  EXPECT_FALSE (l.actModeField.intersects (l.actionField));
  EXPECT_GE (l.actModeField.getX (), l.actionField.getRight ());

  for (auto const &control : l.controls)
    EXPECT_LE (l.actModeField.getBottom (), control.getY ());

  EXPECT_GE (l.actModeField.getWidth (), fingertipSize);
  EXPECT_GE (l.actModeField.getHeight (), fingertipSize);
}

// What the slot fires is named above the controls and clear of them.
TEST (ActionLayout, TheActionFieldSitsClearAboveTheControls)
{
  auto const l = layOutActionPage ({ 0, 0, 768, 300 }, headerSize, 14.f, 1.f);

  ASSERT_FALSE (l.actionField.isEmpty ());
  for (auto const &control : l.controls)
    {
      EXPECT_LE (l.actionField.getBottom (), control.getY ());
      EXPECT_FALSE (l.actionField.intersects (control));
    }
}

// Everything inside the page it was given.
TEST (ActionLayout, NothingEscapesThePage)
{
  auto const page = juce::Rectangle<int>{ 0, 0, 640, 200 };
  auto const l = layOutActionPage (page, headerSize, 14.f, 1.f);

  EXPECT_TRUE (page.contains (l.actionField));
  EXPECT_TRUE (page.contains (l.actModeField));
  for (auto const &control : l.controls)
    EXPECT_TRUE (page.contains (control));
  for (auto const &label : l.rowLabels)
    EXPECT_TRUE (page.contains (label));
}
