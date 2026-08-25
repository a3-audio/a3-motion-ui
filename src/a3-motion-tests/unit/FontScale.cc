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

// The Font Size setting used to reach exactly one component. Everything else —
// the status bar, the menu, every label — kept its size, so "175%" meant
// "175% of the clip settings". The scale has to live where all of them read it.

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

TEST_F (FontScaleTest, SettingTheScaleLeavesTheRestOfTheThemeAlone)
{
  Theme skin;
  skin.accent = { 1, 2, 3 };
  skin.sphereScale = 0.5f;
  setTheme (skin);

  setFontScale (1.75f);

  EXPECT_NEAR (theme ().fontScale, 1.75f, 0.001f);
  EXPECT_EQ (theme ().accent.r, 1) << "a font change must not reload the skin";
  EXPECT_NEAR (theme ().sphereScale, 0.5f, 0.001f);
}

// The other half: a skin reload must not throw away the menu's factor. The two
// arrive from different files at different times.
TEST_F (FontScaleTest, ASkinReloadKeepsTheFactor)
{
  setFontScale (1.5f);

  auto reloaded = loadTheme (juce::var{});
  reloaded.fontScale = theme ().fontScale;
  setTheme (reloaded);

  EXPECT_NEAR (theme ().fontScale, 1.5f, 0.001f);
}

TEST_F (FontScaleTest, EveryRoleGrowsByTheSameFactor)
{
  auto const unscaled = loadTheme (juce::var{});
  setFontScale (1.75f);

  for (auto const role : { FontRole::Heading, FontRole::Label, FontRole::Value,
                           FontRole::Status })
    EXPECT_NEAR (theme ().fontSize (role), unscaled.fontSize (role) * 1.75f,
                 0.01f);
}

// A saved index has to arrive at the theme, or the setting survives a restart
// on paper only — it is written back correctly and then ignored.
TEST_F (FontScaleTest, ASavedIndexReachesTheTheme)
{
  auto const file
      = juce::File::getSpecialLocation (
            juce::File::SpecialLocationType::tempDirectory)
            .getChildFile ("a3-font-scale-test.json");
  file.deleteFile ();

  saveSettings (file, AppSettings{ 0, 1, numFontScales - 1 });
  auto const loaded = loadSettings (file);
  setFontScale (fontScaleForIndex (loaded.fontSizeIndex));

  EXPECT_NEAR (theme ().fontScale, 1.75f, 0.001f);

  file.deleteFile ();
}

// Labels take their size from the LookAndFeel, which is why the status bar
// never moved: nothing in it ever called setFont. Scaling there is what makes
// the setting reach components that never ask for it — including ones added
// later.
TEST_F (FontScaleTest, ALabelGrowsWithoutAskingForIt)
{
  LookAndFeel_A3 lookAndFeel;
  juce::Label label;
  label.setLookAndFeel (&lookAndFeel);

  auto const base = lookAndFeel.getLabelFont (label).getHeight ();

  setFontScale (1.75f);
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

  setFontScale (1.5f);

  EXPECT_NEAR (lookAndFeel.getLabelFont (label).getHeight (), 60.f, 0.5f);

  label.setLookAndFeel (nullptr);
}

}
