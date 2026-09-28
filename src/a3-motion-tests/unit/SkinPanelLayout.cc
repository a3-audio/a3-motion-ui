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

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/components/SkinPanelLayout.hh>
#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

namespace
{
/** The rig's screen, less the status bar and the clip settings bar. */
juce::Rectangle<int> const motionArea{ 0, 0, 768, 620 };

int
countOf (std::vector<SkinPanelRow> const &rows, SkinPanelRowKind kind)
{
  int count = 0;
  for (auto const &row : rows)
    count += row.kind == kind ? 1 : 0;
  return count;
}
}

// Closed, the panel is the title, one header per section and the footer.
TEST (SkinPanelLayout, WithNothingOpenEverySectionIsOneHeader)
{
  auto const rows = skinPanelRows (std::nullopt);

  ASSERT_EQ (rows.size (), skinSections ().size () + 2);
  EXPECT_EQ (rows.front ().kind, SkinPanelRowKind::Title);
  EXPECT_EQ (rows.back ().kind, SkinPanelRowKind::Footer);
  EXPECT_EQ (countOf (rows, SkinPanelRowKind::SectionHeader),
             static_cast<int> (skinSections ().size ()));
}

// An open section's rows stand right under its own header, effects first --
// the switch is what a performer reaches for -- then bars, then colours.
TEST (SkinPanelLayout, AnOpenSectionsRowsFollowItsHeader)
{
  auto const rows = skinPanelRows (SkinSection::Blob);
  auto const &blob = skinSectionSpec (SkinSection::Blob);

  auto header = std::find_if (rows.begin (), rows.end (), [] (auto const &r) {
    return r.kind == SkinPanelRowKind::SectionHeader
           && r.section == SkinSection::Blob;
  });
  ASSERT_NE (header, rows.end ());

  auto it = header + 1;
  for (int i = 0; i < static_cast<int> (blob.effects.size ()); ++i, ++it)
    {
      EXPECT_EQ (it->kind, SkinPanelRowKind::Effect);
      EXPECT_EQ (it->section, SkinSection::Blob);
      EXPECT_EQ (it->index, i);
    }
  for (int i = 0; i < static_cast<int> (blob.values.size ()); ++i, ++it)
    {
      EXPECT_EQ (it->kind, SkinPanelRowKind::Value);
      EXPECT_EQ (it->index, i);
    }
  for (int i = 0; i < static_cast<int> (blob.colours.size ()); ++i, ++it)
    {
      EXPECT_EQ (it->kind, SkinPanelRowKind::Colour);
      EXPECT_EQ (it->index, i);
    }

  EXPECT_EQ (it->kind, SkinPanelRowKind::SectionHeader);
  EXPECT_EQ (it->section, SkinSection::Trajectory);
}

// Only one section open: the others stay a header each.
TEST (SkinPanelLayout, OnlyTheOpenSectionShowsItsRows)
{
  for (auto const &row : skinPanelRows (SkinSection::Sphere))
    if (row.kind == SkinPanelRowKind::Effect
        || row.kind == SkinPanelRowKind::Value
        || row.kind == SkinPanelRowKind::Colour)
      EXPECT_EQ (row.section, SkinSection::Sphere);
}

// "pack ihn nach links": the left edge, full height, and the larger part of
// the screen left to what is being tuned.
TEST (SkinPanelLayout, ThePanelStandsAtTheLeftEdge)
{
  auto const panel = skinPanelBounds (motionArea);

  EXPECT_EQ (panel.getX (), motionArea.getX ());
  EXPECT_EQ (panel.getY (), motionArea.getY ());
  EXPECT_EQ (panel.getHeight (), motionArea.getHeight ());
  EXPECT_LT (panel.getWidth (), motionArea.getWidth () / 2);
  EXPECT_GT (panel.getWidth (), motionArea.getWidth () / 3);
}

TEST (SkinPanelLayout, ARowIsNeverSmallerThanAFingertip)
{
  EXPECT_GE (skinPanelRowHeight (6.f), fingertipSize);
  EXPECT_GT (skinPanelRowHeight (26.f), skinPanelRowHeight (13.75f));
}

// The busiest section fits the rig's panel without scrolling at the largest
// body text any shipped skin carries -- scrolling is what a sideways bar
// cannot share a finger with. Measured on the rig: the sphere's component is
// 590 px tall, and the panel keeps `padding` inside it top and bottom.
TEST (SkinPanelLayout, TheBusiestSectionFitsTheRigsPanel)
{
  auto constexpr rigMotionHeight = 590.f;
  auto constexpr largestShippedBodyText = 15.81f;

  auto const rowHeight = skinPanelRowHeight (largestShippedBodyText);
  auto const room = rigMotionHeight - 2.f * Theme{}.padding;

  for (auto const &spec : skinSections ())
    {
      auto const rows = static_cast<int> (skinPanelRows (spec.section).size ());
      EXPECT_LE (static_cast<float> (rows * rowHeight), room) << spec.label;
    }
}

TEST (SkinPanelLayout, AnEffectRowIsSwitchMinusBarPlus)
{
  juce::Rectangle<int> const row{ 0, 100, 307, 36 };
  auto const parts = skinPanelRowParts (SkinPanelRowKind::Effect, row);

  EXPECT_EQ (parts.toggle.getX (), row.getX ());
  EXPECT_LE (parts.toggle.getRight (), parts.minus.getX ());
  EXPECT_EQ (parts.minus.getRight (), parts.bar.getX ());
  EXPECT_EQ (parts.bar.getRight (), parts.plus.getX ());
  EXPECT_EQ (parts.plus.getRight (), row.getRight ());
  EXPECT_GT (parts.bar.getWidth (), row.getWidth () / 2);

  for (auto const &part : { parts.toggle, parts.minus, parts.bar, parts.plus })
    EXPECT_TRUE (row.contains (part)) << part.toString ();
}

// The step keys are a fingertip wide at least: they are hit, not dragged.
TEST (SkinPanelLayout, TheStepKeysAreAFingertipWide)
{
  juce::Rectangle<int> const row{ 0, 0, 307, fingertipSize };
  auto const parts = skinPanelRowParts (SkinPanelRowKind::Value, row);

  EXPECT_GE (parts.minus.getWidth (), fingertipSize);
  EXPECT_GE (parts.plus.getWidth (), fingertipSize);
}

// One column of bars, switch or no switch in front.
TEST (SkinPanelLayout, EveryBarLinesUpWithEveryOther)
{
  juce::Rectangle<int> const row{ 0, 0, 307, 36 };
  auto const effect = skinPanelRowParts (SkinPanelRowKind::Effect, row);
  auto const value = skinPanelRowParts (SkinPanelRowKind::Value, row);

  EXPECT_EQ (effect.bar, value.bar);
  EXPECT_TRUE (value.toggle.isEmpty ());
}

TEST (SkinPanelLayout, AHeaderIsASwitchAndItsName)
{
  juce::Rectangle<int> const row{ 0, 0, 307, 36 };
  auto const parts = skinPanelRowParts (SkinPanelRowKind::SectionHeader, row);

  EXPECT_EQ (parts.toggle.getX (), row.getX ());
  EXPECT_LE (parts.toggle.getRight (), parts.label.getX ());
  EXPECT_EQ (parts.label.getRight (), row.getRight ());
  EXPECT_TRUE (parts.bar.isEmpty ());
}

// A colour row's swatch sits where a bar would, so the swatches line up with
// the bars above them.
TEST (SkinPanelLayout, AColourRowIsASwatchInTheBarsColumn)
{
  juce::Rectangle<int> const row{ 0, 0, 307, 36 };
  auto const colour = skinPanelRowParts (SkinPanelRowKind::Colour, row);
  auto const value = skinPanelRowParts (SkinPanelRowKind::Value, row);

  EXPECT_EQ (colour.swatch.getX (), value.minus.getX ());
  EXPECT_EQ (colour.swatch.getRight (), value.plus.getRight ());
}

// Nothing reserved: the sphere where it always was.
TEST (SkinPanelLayout, WithoutAnInsetTheSphereIsCentred)
{
  auto const region = sphereRegion (motionArea, 400, 0, 1.4f);
  EXPECT_EQ (region, motionArea.withSizeKeepingCentre (400, 400));
}

// With the panel open it moves into what the panel leaves, at its own size.
TEST (SkinPanelLayout, WithThePanelOpenTheSphereMovesRightAtItsOwnSize)
{
  auto const inset = skinPanelBounds (motionArea).getRight ();
  auto const region = sphereRegion (motionArea, 400, inset, 1.f);

  EXPECT_EQ (region.getWidth (), 400);
  EXPECT_EQ (region.getHeight (), 400);
  EXPECT_EQ (region.getCentreY (), motionArea.getCentreY ());
  EXPECT_EQ (region.getCentreX (), (inset + motionArea.getRight ()) / 2);
}

// The towers stand beside the ball. Where the picture with them is wider than
// what the panel leaves, the whole of it is drawn smaller rather than cut off
// at the screen's edge -- the speakers are three of the six sections.
TEST (SkinPanelLayout, WhatThePanelLeavesHoldsTheWholePicture)
{
  auto const inset = skinPanelBounds (motionArea).getRight ();
  auto const free = motionArea.getRight () - inset;
  auto constexpr reach = 1.5f;

  auto const region = sphereRegion (motionArea, 400, inset, reach);

  EXPECT_LE (static_cast<float> (region.getWidth ()) * reach,
             static_cast<float> (free) + 1.f);
  EXPECT_EQ (region.getWidth (), region.getHeight ());
  EXPECT_EQ (region.getCentreX (), (inset + motionArea.getRight ()) / 2);
}
