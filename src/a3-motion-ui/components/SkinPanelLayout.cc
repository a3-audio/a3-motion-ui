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

#include "SkinPanelLayout.hh"

#include <a3-motion-ui/components/ControllerLayout.hh>
#include <a3-motion-ui/theme/Theme.hh>

#include <cmath>

namespace a3
{

namespace
{
/** A row's height over the body text's: room for one line of it with air
 *  above and below, on the same scale the menu's rows use. */
constexpr float rowHeightOverBodyText = 2.2f;

/** A switch is a pill half again as wide as it is tall -- wide enough to
 *  read as a switch rather than a dot, and to land on without looking. */
constexpr float switchWidthOverHeight = 1.5f;

void
appendSection (std::vector<SkinPanelRow> &rows, SkinSectionSpec const &spec)
{
  auto const count = [] (auto const &items) {
    return static_cast<int> (items.size ());
  };

  for (int i = 0; i < count (spec.effects); ++i)
    rows.push_back ({ SkinPanelRowKind::Effect, spec.section, i });
  for (int i = 0; i < count (spec.values); ++i)
    rows.push_back ({ SkinPanelRowKind::Value, spec.section, i });
  for (int i = 0; i < count (spec.colours); ++i)
    rows.push_back ({ SkinPanelRowKind::Colour, spec.section, i });
}

int
switchWidth (juce::Rectangle<int> row)
{
  return juce::roundToInt (static_cast<float> (row.getHeight ())
                          * switchWidthOverHeight);
}

int
stepKeyWidth (juce::Rectangle<int> row)
{
  return juce::jmax (fingertipSize, row.getHeight ());
}

int
gap ()
{
  return juce::roundToInt (theme ().paddingSmall);
}
}

std::vector<SkinPanelRow>
skinPanelRows (std::optional<SkinSection> open)
{
  std::vector<SkinPanelRow> rows;
  rows.push_back ({ SkinPanelRowKind::Title });

  for (auto const &spec : skinSections ())
    {
      rows.push_back ({ SkinPanelRowKind::SectionHeader, spec.section });
      if (open == spec.section)
        appendSection (rows, spec);
    }

  rows.push_back ({ SkinPanelRowKind::Footer });
  return rows;
}

juce::Rectangle<int>
skinPanelBounds (juce::Rectangle<int> area)
{
  return area.withWidth (area.getWidth () * 2 / 5);
}

int
skinPanelRowHeight (float fontBody)
{
  return juce::jmax (fingertipSize, static_cast<int> (std::ceil (
                                        fontBody * rowHeightOverBodyText)));
}

SkinPanelRowParts
skinPanelRowParts (SkinPanelRowKind kind, juce::Rectangle<int> row)
{
  SkinPanelRowParts parts;

  switch (kind)
    {
    case SkinPanelRowKind::Title:
    case SkinPanelRowKind::Footer:
      parts.label = row;
      return parts;

    case SkinPanelRowKind::SectionHeader:
      {
        auto rest = row;
        parts.toggle = rest.removeFromLeft (switchWidth (row));
        rest.removeFromLeft (gap ());
        parts.label = rest;
        return parts;
      }

    case SkinPanelRowKind::Effect:
    case SkinPanelRowKind::Value:
    case SkinPanelRowKind::Colour:
      break;
    }

  // Effect, value and colour share one column: the switch's width is kept
  // free in front of a bar that has none.
  auto rest = row;
  auto const toggle = rest.removeFromLeft (switchWidth (row));
  rest.removeFromLeft (gap ());

  if (kind == SkinPanelRowKind::Colour)
    {
      parts.swatch = rest;
      parts.label = rest;
      return parts;
    }

  if (kind == SkinPanelRowKind::Effect)
    parts.toggle = toggle;

  parts.minus = rest.removeFromLeft (stepKeyWidth (row));
  parts.plus = rest.removeFromRight (stepKeyWidth (row));
  parts.bar = rest;
  return parts;
}

juce::Rectangle<int>
sphereRegion (juce::Rectangle<int> bounds, int diameter, int leftInset,
              float sceneReach)
{
  if (leftInset <= 0)
    return bounds.withSizeKeepingCentre (diameter, diameter);

  auto const free = bounds.withTrimmedLeft (leftInset);

  // The picture is `sceneReach` radii either side of the centre; where that
  // is wider than what the panel leaves, all of it is drawn smaller.
  auto const fitting = static_cast<int> (
      static_cast<float> (free.getWidth ()) / juce::jmax (1.f, sceneReach));
  auto const shown = juce::jmin (diameter, fitting);

  return juce::Rectangle<int> (shown, shown)
      .withCentre ({ free.getCentreX (), bounds.getCentreY () });
}

}
