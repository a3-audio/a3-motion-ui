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

#include <a3-motion-ui/theme/CleanSkin.hh>

using namespace a3;

namespace
{
juce::StringArray const skins{ "clean", "default", "ember", "sunset" };
}

// The first tap goes to clean and keeps the skin it left, so the second can
// find its way back.
TEST (CleanSkin, ATapGoesToCleanAndRemembersWhereItCameFrom)
{
  auto const toggle = toggleCleanSkin ("ember", "", skins);

  ASSERT_TRUE (toggle.has_value ());
  EXPECT_EQ (toggle->apply, "clean");
  EXPECT_EQ (toggle->remember, "ember");
}

TEST (CleanSkin, TheNextTapGoesBackToIt)
{
  auto const toggle = toggleCleanSkin ("clean", "ember", skins);

  ASSERT_TRUE (toggle.has_value ());
  EXPECT_EQ (toggle->apply, "ember");
  EXPECT_EQ (toggle->remember, "ember");
}

// A skin picked in the menu while clean was up is the one the key leaves from
// next time, not the one remembered from before.
TEST (CleanSkin, ASkinPickedInTheMenuIsTheOneItLeavesFrom)
{
  auto const toggle = toggleCleanSkin ("sunset", "ember", skins);

  ASSERT_TRUE (toggle.has_value ());
  EXPECT_EQ (toggle->apply, "clean");
  EXPECT_EQ (toggle->remember, "sunset");
}

// Deleted or renamed in the editor since, or never remembered at all -- a
// device that started in clean has nothing to go back to.
TEST (CleanSkin, WithNothingToGoBackToItGoesToDefault)
{
  for (auto const *remembered : { "", "gone", "clean" })
    {
      auto const toggle = toggleCleanSkin ("clean", remembered, skins);
      ASSERT_TRUE (toggle.has_value ()) << remembered;
      EXPECT_EQ (toggle->apply, "default") << remembered;
    }
}

TEST (CleanSkin, WithoutADefaultItGoesToTheFirstSkinThereIs)
{
  auto const toggle
      = toggleCleanSkin ("clean", "", juce::StringArray{ "clean", "ember" });

  ASSERT_TRUE (toggle.has_value ());
  EXPECT_EQ (toggle->apply, "ember");
}

TEST (CleanSkin, WithNoCleanSkinATapDoesNothing)
{
  EXPECT_FALSE (
      toggleCleanSkin ("ember", "", juce::StringArray{ "default", "ember" })
          .has_value ());
}

// Clean alone on the device: there is nowhere to go back to, so the tap has
// nothing to do rather than applying the skin that is already up.
TEST (CleanSkin, WithOnlyCleanATapDoesNothing)
{
  EXPECT_FALSE (
      toggleCleanSkin ("clean", "", juce::StringArray{ "clean" }).has_value ());
}
