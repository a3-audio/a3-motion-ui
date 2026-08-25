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

#include <a3-motion-ui/theme/ThemeColours.hh>

using namespace a3;

namespace
{

TEST (ThemeColours, ARoleBecomesTheColourItStates)
{
  auto const colour = toColour (ThemeColour{ 10, 20, 30 });

  EXPECT_EQ (colour.getRed (), 10);
  EXPECT_EQ (colour.getGreen (), 20);
  EXPECT_EQ (colour.getBlue (), 30);
  EXPECT_EQ (colour.getAlpha (), 255) << "roles are opaque; alpha is a state";
}

TEST (ThemeColours, TheBackgroundFollowsTheTheme)
{
  Theme skin;
  skin.background = { 1, 2, 3 };
  setTheme (skin);

  EXPECT_EQ (Colours::background (), juce::Colour (1, 2, 3));

  setTheme (loadTheme (juce::var{}));
}

// statusBar is derived from background rather than being its own role: it is a
// rule, not a value, and two independent tokens would drift apart.
TEST (ThemeColours, TheStatusBarStaysDerivedFromTheBackground)
{
  Theme skin;
  skin.background = { 200, 100, 50 };
  setTheme (skin);

  auto const derived = Colours::statusBar ();
  EXPECT_NE (derived, Colours::background ())
      << "the status bar has to be told apart from the ground";
  EXPECT_NEAR (derived.getHue (), Colours::background ().getHue (), 0.02f)
      << "it is the same colour at a different lightness, not another colour";

  setTheme (loadTheme (juce::var{}));
}

}
