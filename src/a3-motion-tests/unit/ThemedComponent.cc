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

#include <a3-motion-ui/theme/ThemedComponent.hh>

using namespace a3;

namespace
{

// Most components read the theme while painting, so a repaint is all a skin
// change needs. Some cannot: the status bar draws with juce::Labels, which
// take their colour and font once and keep them. Under a new skin those
// labels kept the old skin's accent — the status bar was the one part of the
// screen a skin could not reach.

struct CountingComponent : public juce::Component, public ThemedComponent
{
  void
  applyTheme () override
  {
    ++applied;
  }

  int applied = 0;
};

TEST (ThemedComponentTest, TheWalkReachesADirectChild)
{
  juce::Component root;
  CountingComponent child;
  root.addChildComponent (child);

  applyThemeToTree (root);

  EXPECT_EQ (child.applied, 1);
}

// The status bar is not a direct child of whatever triggers the reload, and
// neither is anything else that will need this later.
TEST (ThemedComponentTest, TheWalkReachesADeeplyNestedChild)
{
  juce::Component root;
  juce::Component middle;
  CountingComponent deep;

  root.addChildComponent (middle);
  middle.addChildComponent (deep);

  applyThemeToTree (root);

  EXPECT_EQ (deep.applied, 1);
}

// A hidden component still has to be told: it can be shown again later, and
// nothing would tell it then.
TEST (ThemedComponentTest, AHiddenChildIsToldToo)
{
  juce::Component root;
  CountingComponent hidden;
  root.addChildComponent (hidden);
  hidden.setVisible (false);

  applyThemeToTree (root);

  EXPECT_EQ (hidden.applied, 1);
}

TEST (ThemedComponentTest, TheRootItselfIsToldWhenItWantsToBe)
{
  CountingComponent root;

  applyThemeToTree (root);

  EXPECT_EQ (root.applied, 1);
}

// Every other component in the tree is left alone — the walk must not depend
// on a component opting out.
TEST (ThemedComponentTest, ComponentsThatDoNotCacheAreUntouched)
{
  juce::Component root;
  juce::Component plain;
  CountingComponent counting;

  root.addChildComponent (plain);
  root.addChildComponent (counting);

  EXPECT_NO_THROW (applyThemeToTree (root));
  EXPECT_EQ (counting.applied, 1);
}

}
