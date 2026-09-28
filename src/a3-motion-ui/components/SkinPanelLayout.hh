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

#include <JuceHeader.h>

#include <a3-motion-ui/theme/SkinSections.hh>

#include <optional>
#include <vector>

namespace a3
{

/** Where the skin panel stands and how its rows are cut up.
 *
 *  Pure functions with a test, like the other `*Layout` files, so the
 *  components are placed by the same arithmetic the test checks.
 *
 *  The panel stands at the **left edge**, not over the middle: *"der
 *  skineditor liegt komplett über der einzustellenden fläche. pack ihn nach
 *  links."* The old editor filled the sphere's whole component with a scrim
 *  and a panel across its middle, so every value was set blind and checked
 *  by closing it. The sphere moves over into what the panel leaves
 *  (`sphereRegion`). */

/** What one row of the panel is. */
enum class SkinPanelRowKind
{
  /** The skin's name. */
  Title,
  /** A section's switch and name; a tap on the name opens it. */
  SectionHeader,
  /** An effect's switch, and the bar for its amount. */
  Effect,
  /** A bar with no switch. */
  Value,
  /** A swatch that opens the colour picker. */
  Colour,
  /** The way to the full list, and the skin's file actions. */
  Footer,
};

struct SkinPanelRow
{
  SkinPanelRowKind kind;
  SkinSection section = SkinSection::Sphere;
  /** Into the section's effects, values or colours, by kind. */
  int index = -1;
};

/** The rows, top to bottom: the title, every section's header, the open
 *  section's rows right under its own header, and the footer.
 *
 *  One section open at a time. With every section's rows at once the panel
 *  is a list again, and scrolling a list is what a bar that is dragged
 *  sideways cannot share a finger with. Seven headers and one section's
 *  worth of rows fit the panel's height at the shipped type size. */
std::vector<SkinPanelRow> skinPanelRows (std::optional<SkinSection> open);

/** The panel inside the sphere's component: the left edge, full height, two
 *  fifths of the width. */
juce::Rectangle<int> skinPanelBounds (juce::Rectangle<int> area);

/** How tall a row is: from the body text, never below a fingertip. */
int skinPanelRowHeight (float fontBody);

/** The pieces of a row. What a kind has no use for stays empty. */
struct SkinPanelRowParts
{
  juce::Rectangle<int> toggle;
  juce::Rectangle<int> minus;
  juce::Rectangle<int> bar;
  juce::Rectangle<int> plus;
  juce::Rectangle<int> swatch;
  juce::Rectangle<int> label;
};

/** Every bar in the panel starts at the same x and ends at the same x,
 *  switch or no switch in front of it, so the eye reads them as one column
 *  of levels. */
SkinPanelRowParts skinPanelRowParts (SkinPanelRowKind kind,
                                     juce::Rectangle<int> row);

/** Where the sphere is drawn: `diameter` across, centred in what is left of
 *  `bounds` right of `leftInset`. Zero puts it where it has always been, in
 *  the middle, at its own size.
 *
 *  `sceneReach` is how far the picture reaches either side of the centre, in
 *  sphere radii -- the towers stand beside the ball
 *  (`speakerSceneReach()`). Beside the panel, a picture wider than what is
 *  left is drawn smaller as a whole rather than cut off at the edge: the
 *  speakers are three of the sections being tuned. */
juce::Rectangle<int> sphereRegion (juce::Rectangle<int> bounds, int diameter,
                                   int leftInset, float sceneReach);

}
