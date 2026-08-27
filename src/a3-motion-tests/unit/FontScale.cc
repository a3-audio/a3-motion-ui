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

#include <a3-motion-ui/SettingsPersistence.hh>
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

// There are two sizes on this screen and no more: the header — the status bar
// and a section's title — and the body, which is every setting and every value
// under it. Four roles said the same thing in a way nobody could set from the
// menu, and left the status bar larger than the headings it sits above.

TEST_F (FontScaleTest, TheMenuIndexPicksAFactor)
{
  EXPECT_NEAR (fontScaleForIndex (1), 1.f, 0.001f) << "index 1 is 100%";
  EXPECT_NEAR (fontScaleForIndex (numFontScales - 1), 1.75f, 0.001f);
}

// An index out of range must not read past the table. It arrives from a saved
// file, which can be older than the table or hand-edited.
TEST_F (FontScaleTest, AnIndexOutOfRangeFallsBackToUnscaled)
{
  EXPECT_NEAR (fontScaleForIndex (-1), 1.f, 0.001f);
  EXPECT_NEAR (fontScaleForIndex (numFontScales + 5), 1.f, 0.001f);
}

// The point of splitting them: one setting must not move the other.
TEST_F (FontScaleTest, TheTwoScalesAreIndependent)
{
  auto const unscaled = loadTheme (juce::var{});

  setHeaderScale (1.75f);

  EXPECT_NEAR (theme ().fontSize (FontRole::Header),
               unscaled.fontSize (FontRole::Header) * 1.75f, 0.01f);
  EXPECT_NEAR (theme ().fontSize (FontRole::Body),
               unscaled.fontSize (FontRole::Body), 0.01f);

  setBodyScale (0.75f);

  EXPECT_NEAR (theme ().fontSize (FontRole::Header),
               unscaled.fontSize (FontRole::Header) * 1.75f, 0.01f)
      << "setting the body scale must not disturb the header";
  EXPECT_NEAR (theme ().fontSize (FontRole::Body),
               unscaled.fontSize (FontRole::Body) * 0.75f, 0.01f);
}

TEST_F (FontScaleTest, SettingAScaleLeavesTheRestOfTheThemeAlone)
{
  Theme skin;
  skin.accent = { 1, 2, 3 };
  skin.sphereScale = 0.5f;
  setTheme (skin);

  setBodyScale (1.75f);

  EXPECT_NEAR (theme ().bodyScale, 1.75f, 0.001f);
  EXPECT_EQ (theme ().accent.r, 1) << "a font change must not reload the skin";
  EXPECT_NEAR (theme ().sphereScale, 0.5f, 0.001f);
}

// The other half: a skin reload must not throw away the menu's factors. The
// two arrive from different files at different times.
TEST_F (FontScaleTest, ASkinReloadKeepsBothFactors)
{
  setHeaderScale (1.5f);
  setBodyScale (0.75f);

  auto reloaded = loadTheme (juce::var{});
  reloaded.headerScale = theme ().headerScale;
  reloaded.bodyScale = theme ().bodyScale;
  setTheme (reloaded);

  EXPECT_NEAR (theme ().headerScale, 1.5f, 0.001f);
  EXPECT_NEAR (theme ().bodyScale, 0.75f, 0.001f);
}

// Saved indices have to arrive at the theme, or the settings survive a restart
// on paper only — written back correctly and then ignored.
TEST_F (FontScaleTest, SavedIndicesReachTheTheme)
{
  auto const file
      = juce::File::getSpecialLocation (
            juce::File::SpecialLocationType::tempDirectory)
            .getChildFile ("a3-font-scale-test.json");
  file.deleteFile ();

  saveSettings (file, AppSettings{ 0, 1, numFontScales - 1, 0 });
  auto const loaded = loadSettings (file);

  EXPECT_EQ (loaded.headerSizeIndex, numFontScales - 1);
  EXPECT_EQ (loaded.bodySizeIndex, 0);

  file.deleteFile ();
}

// A settings file written before the split carries one index for both. Reading
// it as "header only" would silently shrink every caption on the device.
TEST_F (FontScaleTest, AnOlderSettingsFileSetsBothSizes)
{
  auto const file
      = juce::File::getSpecialLocation (
            juce::File::SpecialLocationType::tempDirectory)
            .getChildFile ("a3-font-scale-legacy.json");
  file.deleteFile ();
  file.replaceWithText ("{\"clockMode\": 0, \"potSizeIndex\": 1, "
                        "\"fontSizeIndex\": 4}");

  auto const loaded = loadSettings (file);

  EXPECT_EQ (loaded.headerSizeIndex, 4);
  EXPECT_EQ (loaded.bodySizeIndex, 4);

  file.deleteFile ();
}

// Labels take their size from the LookAndFeel, which is why the status bar
// never moved: nothing in it ever called setFont. Everything JUCE draws for us
// — menus, buttons, plain labels — is body text.
TEST_F (FontScaleTest, ALabelGrowsWithTheBodyScale)
{
  LookAndFeel_A3 lookAndFeel;
  juce::Label label;
  label.setLookAndFeel (&lookAndFeel);

  auto const base = lookAndFeel.getLabelFont (label).getHeight ();

  setBodyScale (1.75f);
  auto const scaled = lookAndFeel.getLabelFont (label).getHeight ();

  EXPECT_NEAR (scaled, base * 1.75f, 0.5f);

  label.setLookAndFeel (nullptr);
}

// A label that states its own size keeps that size as its base — the factor
// multiplies it rather than replacing it.
TEST_F (FontScaleTest, AnExplicitFontKeepsItsProportion)
{
  LookAndFeel_A3 lookAndFeel;
  juce::Label label;
  label.setLookAndFeel (&lookAndFeel);
  label.setFont (juce::Font (juce::FontOptions (40.f)));

  setBodyScale (1.5f);

  EXPECT_NEAR (lookAndFeel.getLabelFont (label).getHeight (), 60.f, 0.5f);

  label.setLookAndFeel (nullptr);
}

}
