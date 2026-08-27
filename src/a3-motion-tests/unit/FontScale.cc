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

#include <a3-motion-ui/components/LookAndFeel.hh>

using namespace a3;

namespace
{

struct FontScaleTest : public ::testing::Test
{
  void
  TearDown () override
  {
    setTheme (loadTheme (juce::var{}));
  }
};

// A handful of places draw with JUCE's own default fonts rather than asking
// the theme for a role — juce::Label above all. They follow the skin through
// scaleFor(), which is the skin's size against the built-in one. Until
// 2026-08-28 they followed a percentage set from the menu instead, which lived
// beside the skin's own size and did not travel with it.

TEST_F (FontScaleTest, ALabelFollowsTheSkinsBodySize)
{
  LookAndFeel_A3 lookAndFeel;
  juce::Label label;
  label.setLookAndFeel (&lookAndFeel);

  auto const base = lookAndFeel.getLabelFont (label).getHeight ();

  // 15 is the built-in body size, so this skin is exactly 1.75x it.
  setTheme (loadTheme (juce::JSON::parse (R"({"fontBody": 26.25})")));
  auto const scaled = lookAndFeel.getLabelFont (label).getHeight ();

  EXPECT_NEAR (scaled, base * 1.75f, 0.5f);

  label.setLookAndFeel (nullptr);
}

// A label that states its own size keeps that size as its base — the skin's
// ratio multiplies it rather than replacing it.
TEST_F (FontScaleTest, AnExplicitFontKeepsItsProportion)
{
  LookAndFeel_A3 lookAndFeel;
  juce::Label label;
  label.setLookAndFeel (&lookAndFeel);
  label.setFont (juce::Font (juce::FontOptions (40.f)));

  setTheme (loadTheme (juce::JSON::parse (R"({"fontBody": 22.5})")));

  EXPECT_NEAR (lookAndFeel.getLabelFont (label).getHeight (), 60.f, 0.5f);

  label.setLookAndFeel (nullptr);
}

// Switching skin has to move these too, or they stay at the old skin's size
// while everything asked of the theme by role has already moved.
TEST_F (FontScaleTest, SwitchingSkinMovesTheLabelsWithIt)
{
  LookAndFeel_A3 lookAndFeel;
  juce::Label label;
  label.setLookAndFeel (&lookAndFeel);

  setTheme (loadTheme (juce::JSON::parse (R"({"fontBody": 12.0})")));
  auto const small = lookAndFeel.getLabelFont (label).getHeight ();

  setTheme (loadTheme (juce::JSON::parse (R"({"fontBody": 26.0})")));
  auto const large = lookAndFeel.getLabelFont (label).getHeight ();

  EXPECT_GT (large, small);

  label.setLookAndFeel (nullptr);
}

}
