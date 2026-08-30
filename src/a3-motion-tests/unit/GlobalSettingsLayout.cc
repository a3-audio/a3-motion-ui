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

#include <a3-motion-ui/components/GlobalSettingsComponent.hh>

using namespace a3;

namespace
{
// The menu covers the sphere, not the clip settings bar below it.
juce::Rectangle<int> const menuArea{ 0, 0, 768, 700 };
constexpr int numOptions = 7;
}

TEST (GlobalSettingsLayout, RowsStackWithoutOverlapping)
{
  auto const panel = globalSettingsPanelBounds (menuArea, numOptions);

  for (int i = 0; i < numOptions; ++i)
    for (int j = i + 1; j < numOptions; ++j)
      EXPECT_TRUE (globalSettingsRowBounds (panel, numOptions, i)
                       .getIntersection (
                           globalSettingsRowBounds (panel, numOptions, j))
                       .isEmpty ())
          << "rows " << i << " and " << j << " overlap";
}

TEST (GlobalSettingsLayout, EveryRowStaysInsideThePanel)
{
  auto const panel = globalSettingsPanelBounds (menuArea, numOptions);

  for (int i = 0; i < numOptions; ++i)
    EXPECT_TRUE (panel.contains (globalSettingsRowBounds (panel, numOptions, i)))
        << "row " << i << " escapes the panel";
}

TEST (GlobalSettingsLayout, RowsAreOrderedTopToBottom)
{
  auto const panel = globalSettingsPanelBounds (menuArea, numOptions);

  for (int i = 1; i < numOptions; ++i)
    EXPECT_GT (globalSettingsRowBounds (panel, numOptions, i).getY (),
               globalSettingsRowBounds (panel, numOptions, i - 1).getY ());
}

// The name is tapped to browse a row, the value field to arm it — so the two
// have to be separable, and together cover the row.
TEST (GlobalSettingsLayout, TheNameAndTheValueSplitTheRow)
{
  auto const panel = globalSettingsPanelBounds (menuArea, numOptions);
  auto const row = globalSettingsRowBounds (panel, numOptions, 0);

  auto const name = globalSettingsNameArea (row);
  auto const value = globalSettingsValueArea (row);

  EXPECT_TRUE (name.getIntersection (value).isEmpty ());
  EXPECT_TRUE (row.contains (name));
  EXPECT_TRUE (row.contains (value));
  EXPECT_LT (name.getX (), value.getX ());
}

// A menu that outgrows the space it is given must not lay its rows outside
// it — the panel is centred in whatever the sphere leaves.
TEST (GlobalSettingsLayout, ManyRowsStillFitTheirPanel)
{
  for (int count : { 1, 3, 7, 12, 20 })
    {
      auto const panel = globalSettingsPanelBounds (menuArea, count);

      for (int i = 0; i < count; ++i)
        EXPECT_TRUE (panel.contains (globalSettingsRowBounds (panel, count, i)))
            << count << " options, row " << i;
    }
}
