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

#include <a3-motion-ui/components/SkinPanelComponent.hh>
#include <a3-motion-ui/theme/SkinParameters.hh>
#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

namespace
{
juce::var
skinFrom (char const *json)
{
  return juce::JSON::parse (juce::String (json));
}

SkinTunable const &
effectAmount (SkinSection section, int effect)
{
  return skinSectionSpec (section).effects[static_cast<size_t> (effect)].amount;
}

int
effectIndex (SkinSection section, char const *key)
{
  auto const &effects = skinSectionSpec (section).effects;
  for (size_t i = 0; i < effects.size (); ++i)
    if (juce::String (effects[i].key) == key)
      return static_cast<int> (i);
  return -1;
}
}

// A switch is written into the skin, and the caller hears about it so the
// sphere changes while the finger is still there.
TEST (SkinPanel, AnEffectSwitchWritesTheSkinAndSaysSo)
{
  SkinPanelComponent panel;
  panel.setSkin (skinFrom (R"({ "lineGlow": 0.5 })"), "test");

  int changes = 0;
  panel.onValueChanged = [&] { ++changes; };

  auto const glow = effectIndex (SkinSection::Trajectory, "glow");
  panel.setEffectSwitch (SkinSection::Trajectory, glow, false);

  EXPECT_FALSE (skinSwitchIsOn (
      panel.getSkin (), effectSwitchPath (SkinSection::Trajectory, "glow")));
  EXPECT_EQ (changes, 1);
  // The value stays what it was: the switch decides, the number is kept.
  EXPECT_DOUBLE_EQ (skinValue (panel.getSkin (), "lineGlow"), 0.5);
}

// An effect that is not drawn has nothing to tune: its bar rests.
TEST (SkinPanel, AnEffectsBarRestsWhileItIsOff)
{
  SkinPanelComponent panel;
  panel.setSkin (skinFrom (R"({})"), "test");

  auto const sparks = effectIndex (SkinSection::Blob, "sparks");
  EXPECT_TRUE (panel.isEffectBarEnabled (SkinSection::Blob, sparks));

  panel.setEffectSwitch (SkinSection::Blob, sparks, false);
  EXPECT_FALSE (panel.isEffectBarEnabled (SkinSection::Blob, sparks));

  panel.setEffectSwitch (SkinSection::Blob, sparks, true);
  panel.setSectionSwitch (SkinSection::Blob, false);
  EXPECT_FALSE (panel.isEffectBarEnabled (SkinSection::Blob, sparks));
}

// A bar writes its value into the skin, held inside the bar's range.
TEST (SkinPanel, ABarWritesItsValueWithinItsRange)
{
  SkinPanelComponent panel;
  panel.setSkin (skinFrom (R"({ "lineGlow": 0.5 })"), "test");

  auto const &glow
      = effectAmount (SkinSection::Trajectory,
                      effectIndex (SkinSection::Trajectory, "glow"));

  panel.setTunable (glow, 1.25);
  EXPECT_NEAR (skinValue (panel.getSkin (), "lineGlow"), 1.25, 1e-9);

  panel.setTunable (glow, 99.0);
  EXPECT_NEAR (skinValue (panel.getSkin (), "lineGlow"), glow.max, 1e-9);
}

// − and + move a hundredth of the bar, from what the skin holds.
TEST (SkinPanel, TheStepKeysMoveAHundredthOfTheBar)
{
  SkinPanelComponent panel;
  panel.setSkin (skinFrom (R"({ "lineGlow": 1.0 })"), "test");

  auto const &glow
      = effectAmount (SkinSection::Trajectory,
                      effectIndex (SkinSection::Trajectory, "glow"));

  panel.stepTunable (glow, 1);
  EXPECT_NEAR (skinValue (panel.getSkin (), "lineGlow"), 1.02, 1e-9);

  panel.stepTunable (glow, -2);
  EXPECT_NEAR (skinValue (panel.getSkin (), "lineGlow"), 0.98, 1e-9);
}

// A value the file never stated reads as what the app draws with, not as 0,
// so the first step starts from what is on screen.
TEST (SkinPanel, AnUnstatedValueStartsFromTheThemesDefault)
{
  SkinPanelComponent panel;
  panel.setSkin (skinFrom (R"({})"), "test");

  auto const &glow
      = effectAmount (SkinSection::Trajectory,
                      effectIndex (SkinSection::Trajectory, "glow"));

  EXPECT_NEAR (panel.tunableValue (glow), 1.0, 1e-9);
}

// One section open at a time; the same header again closes it.
TEST (SkinPanel, OneSectionOpensAtATime)
{
  SkinPanelComponent panel;
  panel.setSkin (skinFrom (R"({})"), "test");

  panel.openSection (SkinSection::Blob);
  EXPECT_EQ (panel.openedSection (), SkinSection::Blob);

  panel.openSection (SkinSection::Sphere);
  EXPECT_EQ (panel.openedSection (), SkinSection::Sphere);

  panel.openSection (std::nullopt);
  EXPECT_FALSE (panel.openedSection ().has_value ());
}

// A colour the file names is what the swatch shows.
TEST (SkinPanel, ASwatchShowsTheFilesColour)
{
  SkinPanelComponent panel;
  panel.setSkin (skinFrom (R"({ "sphereRim": { "r": 10, "g": 20, "b": 30 } })"),
                 "test");

  auto const colour = panel.colourAt ("sphereRim");
  EXPECT_EQ (colour.getRed (), 10);
  EXPECT_EQ (colour.getGreen (), 20);
  EXPECT_EQ (colour.getBlue (), 30);
}

// Opening the panel writes nothing: only a change does.
TEST (SkinPanel, LookingChangesNothing)
{
  auto const skin = skinFrom (R"({ "lineGlow": 0.5, "blobBolt": 1.5 })");
  auto const before = juce::JSON::toString (skin, true);

  SkinPanelComponent panel;
  panel.setSkin (skin, "test");
  for (auto const &spec : skinSections ())
    panel.openSection (spec.section);
  panel.setBounds (0, 0, 307, 620);

  EXPECT_EQ (juce::JSON::toString (panel.getSkin (), true), before);
}

// ── Closing without a change (#53) ───────────────────────────────────────

TEST (SkinUnchanged, TheSameSkinIsTheSameWhateverTheKeyOrder)
{
  EXPECT_TRUE (sameSkin (skinFrom (R"({ "a": 1, "b": { "c": 2 } })"),
                         skinFrom (R"({ "b": { "c": 2 }, "a": 1 })")));
}

TEST (SkinUnchanged, AChangedValueOrAnAddedKeyIsAChange)
{
  auto const skin = skinFrom (R"({ "a": 1, "b": { "c": 2 } })");
  EXPECT_FALSE (sameSkin (skin, skinFrom (R"({ "a": 1, "b": { "c": 3 } })")));
  EXPECT_FALSE (sameSkin (skin, skinFrom (R"({ "a": 1, "b": { "c": 2 }, "d": 0 })")));
  EXPECT_FALSE (sameSkin (skinFrom (R"({ "a": 1, "b": { "c": 2 }, "d": 0 })"), skin));
}

// Opened and closed with nothing touched: the panel hands back what it got.
TEST (SkinUnchanged, APanelLookedAtIsUnchanged)
{
  auto const skin = skinFrom (R"({ "lineGlow": 0.5, "sphereGrid": 1 })");
  auto const opened = copyOfSkin (skin);
  SkinPanelComponent panel;
  panel.setSkin (skin, "test");
  EXPECT_TRUE (sameSkin (opened, panel.getSkin ()));
}

// A switch flipped on the panel is a change, even though the panel edits the
// very object it was given -- which is why the opened skin is a copy.
TEST (SkinUnchanged, APanelEditIsAChangeAgainstTheCopy)
{
  auto const skin = skinFrom (R"({ "lineGlow": 0.5 })");
  auto const opened = copyOfSkin (skin);
  SkinPanelComponent panel;
  panel.setSkin (skin, "test");
  panel.setEffectSwitch (SkinSection::Trajectory,
                         effectIndex (SkinSection::Trajectory, "glow"), false);
  EXPECT_FALSE (sameSkin (opened, panel.getSkin ()));
}
