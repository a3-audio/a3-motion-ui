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

#include <a3-motion-ui/theme/SkinParameters.hh>
#include <a3-motion-ui/theme/SkinSections.hh>
#include <a3-motion-ui/theme/Theme.hh>

#include <set>

using namespace a3;

namespace
{
juce::var
parse (char const *json)
{
  return juce::JSON::parse (juce::String (json));
}

juce::File
shippedSkinsDir ()
{
  return juce::File (A3_CONFIG_JSON_PATH).getParentDirectory ().getChildFile (
      "skins");
}

juce::String
asJson (juce::var const &value)
{
  return juce::JSON::toString (value, true);
}
}

// The six the maintainer named, in the order the eye meets them, and one for
// what belongs to none of them.
TEST (SkinSections, TheSixSectionsThenInterface)
{
  auto const &sections = skinSections ();
  ASSERT_EQ (sections.size (), 7u);

  EXPECT_EQ (sections[0].section, SkinSection::Sphere);
  EXPECT_EQ (sections[1].section, SkinSection::Background);
  EXPECT_EQ (sections[2].section, SkinSection::SpeakerTops);
  EXPECT_EQ (sections[3].section, SkinSection::SpeakerBass);
  EXPECT_EQ (sections[4].section, SkinSection::Blob);
  EXPECT_EQ (sections[5].section, SkinSection::Trajectory);
  EXPECT_EQ (sections[6].section, SkinSection::Interface);
}

// "jede sektion benötigt als erstes eine option an/aus" -- all but the one
// that holds fonts and channel colours, which cannot be off.
TEST (SkinSections, EveryPictureSectionHasASwitchAndEffects)
{
  for (auto const &spec : skinSections ())
    {
      if (spec.section == SkinSection::Interface)
        {
          EXPECT_FALSE (spec.hasSwitch);
          EXPECT_TRUE (spec.effects.empty ());
          continue;
        }

      EXPECT_TRUE (spec.hasSwitch) << spec.label;
      EXPECT_FALSE (spec.effects.empty ()) << spec.label;
    }
}

// The request was to reduce strongly. The old editor showed every value the
// skin has -- over a hundred rows. Counted here so a quiet re-growth fails.
TEST (SkinSections, ThePanelOffersFarFewerValuesThanTheSkinHas)
{
  size_t rows = 0;
  for (auto const &spec : skinSections ())
    rows += spec.effects.size () + spec.values.size () + spec.colours.size ();

  auto const shipped = shippedSkinsDir ().getChildFile ("default.json");
  auto const everything
      = skinParameters (juce::JSON::parse (shipped.loadFileAsString ())).size ();

  EXPECT_LE (rows, 40u);
  EXPECT_LT (rows * 2, everything);
}

// A key named twice would be one value with two bars, and the second one
// would appear to do nothing whenever the first had moved it.
TEST (SkinSections, NoValueIsOfferedTwice)
{
  std::set<std::string> seen;
  for (auto const &spec : skinSections ())
    {
      for (auto const &effect : spec.effects)
        EXPECT_TRUE (seen.insert (effect.amount.path).second)
            << effect.amount.path;
      for (auto const &value : spec.values)
        EXPECT_TRUE (seen.insert (value.path).second) << value.path;
      for (auto const &colour : spec.colours)
        EXPECT_TRUE (seen.insert (colour.path).second) << colour.path;
    }
}

// Every effect's own amount is among what its switch zeroes -- otherwise
// the switch would leave on exactly the thing its row is tuning.
TEST (SkinSections, AnEffectsSwitchCoversItsOwnAmount)
{
  for (auto const &spec : skinSections ())
    for (auto const &effect : spec.effects)
      {
        auto covered = false;
        for (auto const &override_ : effect.whileOff)
          covered = covered
                    || juce::String (override_.path) == effect.amount.path;
        EXPECT_TRUE (covered) << effect.key;
      }
}

// A bar's range has to hold what ships, or the first touch jumps it.
TEST (SkinSections, EveryBarsRangeHoldsTheShippedSkinsValues)
{
  auto const files = shippedSkinsDir ().findChildFiles (
      juce::File::findFiles, false, "*.json");
  ASSERT_FALSE (files.isEmpty ());

  for (auto const &file : files)
    {
      auto const skin
          = migrateSkinNames (juce::JSON::parse (file.loadFileAsString ()));

      auto const check = [&] (SkinTunable const &tunable) {
        if (!skinHasValue (skin, tunable.path))
          return;
        auto const value = skinValue (skin, tunable.path);
        EXPECT_GE (value, tunable.min)
            << tunable.path << " in " << file.getFileName ();
        EXPECT_LE (value, tunable.max)
            << tunable.path << " in " << file.getFileName ();
      };

      for (auto const &spec : skinSections ())
        {
          for (auto const &effect : spec.effects)
            check (effect.amount);
          for (auto const &value : spec.values)
            check (value);
        }
    }
}

TEST (SkinSections, SwitchesLiveInTheirOwnBlock)
{
  EXPECT_EQ (sectionSwitchPath (SkinSection::Sphere), "switches.sphere.on");
  EXPECT_EQ (effectSwitchPath (SkinSection::SpeakerBass, "ballLightning"),
             "switches.speakerBass.ballLightning");
}

// The rule every older skin depends on.
TEST (SkinSections, AMissingSwitchIsOn)
{
  auto const skin = parse (R"({ "lineGlow": 0.5 })");

  EXPECT_TRUE (skinSwitchIsOn (skin, sectionSwitchPath (SkinSection::Blob)));
  EXPECT_TRUE (skinEffectIsOn (skin, SkinSection::Trajectory, "glow"));
  EXPECT_TRUE (skinSwitchIsOn (juce::var (), "switches.sphere.on"));
}

TEST (SkinSections, ASwitchCanBeTurnedOffAndBackOn)
{
  auto skin = parse (R"({ "lineGlow": 0.5 })");
  auto const path = effectSwitchPath (SkinSection::Trajectory, "glow");

  setSkinSwitch (skin, path, false);
  EXPECT_FALSE (skinSwitchIsOn (skin, path));
  EXPECT_FALSE (skinEffectIsOn (skin, SkinSection::Trajectory, "glow"));

  setSkinSwitch (skin, path, true);
  EXPECT_TRUE (skinSwitchIsOn (skin, path));

  // Its neighbour is untouched.
  EXPECT_DOUBLE_EQ (skinValue (skin, "lineGlow"), 0.5);
}

// A section switch rules its effects without touching their own switches:
// on again, each effect comes back as it was left.
TEST (SkinSections, ASectionSwitchRulesItsEffectsWithoutForgettingThem)
{
  auto skin = parse (R"({})");
  setSkinSwitch (skin, effectSwitchPath (SkinSection::Blob, "sparks"), false);
  setSkinSwitch (skin, sectionSwitchPath (SkinSection::Blob), false);

  EXPECT_FALSE (skinEffectIsOn (skin, SkinSection::Blob, "trail"));
  EXPECT_FALSE (skinEffectIsOn (skin, SkinSection::Blob, "sparks"));

  setSkinSwitch (skin, sectionSwitchPath (SkinSection::Blob), true);
  EXPECT_TRUE (skinEffectIsOn (skin, SkinSection::Blob, "trail"));
  EXPECT_FALSE (skinEffectIsOn (skin, SkinSection::Blob, "sparks"));
}

// **The protection the maintainer's skins rely on.** Loading a skin without
// the new keys changes nothing: not the values the renderers get, and not the
// theme that comes out of them. Every shipped skin, as it is on disk.
TEST (SkinSections, ASkinWithoutSwitchesRendersExactlyAsBefore)
{
  auto const files = shippedSkinsDir ().findChildFiles (
      juce::File::findFiles, false, "*.json");
  ASSERT_FALSE (files.isEmpty ());

  for (auto const &file : files)
    {
      auto const skin
          = migrateSkinNames (juce::JSON::parse (file.loadFileAsString ()));
      ASSERT_FALSE (skin.hasProperty ("switches")) << file.getFileName ();

      auto const applied = withSkinSwitchesApplied (skin);
      EXPECT_EQ (asJson (applied), asJson (skin)) << file.getFileName ();

      auto const before = loadTheme (skin);
      auto const after = loadTheme (applied);
      EXPECT_FLOAT_EQ (after.lineGlow, before.lineGlow);
      EXPECT_FLOAT_EQ (after.blobSparkle, before.blobSparkle);
      EXPECT_FLOAT_EQ (after.braidRadius, before.braidRadius);
    }
}

// Every switch on says the same as no switches at all.
TEST (SkinSections, EverySwitchOnIsTheSameAsNone)
{
  auto const skin = parse (R"({ "lineGlow": 0.5, "blobBolt": 1.5,
                                "speakerLight": { "ballLevel": 0.7 } })");
  auto switched = skin.clone ();
  for (auto const &spec : skinSections ())
    {
      if (spec.hasSwitch)
        setSkinSwitch (switched, sectionSwitchPath (spec.section), true);
      for (auto const &effect : spec.effects)
        setSkinSwitch (switched, effectSwitchPath (spec.section, effect.key),
                       true);
    }

  auto const applied = withSkinSwitchesApplied (switched);
  EXPECT_DOUBLE_EQ (skinValue (applied, "lineGlow"), 0.5);
  EXPECT_DOUBLE_EQ (skinValue (applied, "blobBolt"), 1.5);
  EXPECT_DOUBLE_EQ (skinValue (applied, "speakerLight.ballLevel"), 0.7);
}

// Off writes the effect's "off" into what the renderers get -- and only
// there: the skin the editor holds keeps its number, so on brings it back.
TEST (SkinSections, AnEffectOffZeroesWhatTheRenderersGetButNotTheFile)
{
  auto skin = parse (R"({ "speakerLight": { "ballLevel": 0.7, "subGlow": 0.8 } })");
  setSkinSwitch (skin,
                 effectSwitchPath (SkinSection::SpeakerBass, "ballLightning"),
                 false);

  auto const applied = withSkinSwitchesApplied (skin);

  EXPECT_DOUBLE_EQ (skinValue (applied, "speakerLight.ballLevel"), 0.0);
  EXPECT_DOUBLE_EQ (skinValue (applied, "speakerLight.subGlow"), 0.8);
  EXPECT_DOUBLE_EQ (skinValue (skin, "speakerLight.ballLevel"), 0.7);
}

// Off holds even where the file never stated the value: the renderer's own
// default would otherwise draw it at full strength.
TEST (SkinSections, OffHoldsForAValueTheFileNeverStated)
{
  auto skin = parse (R"({})");
  setSkinSwitch (skin, effectSwitchPath (SkinSection::Trajectory, "glow"),
                 false);

  auto const applied = withSkinSwitchesApplied (skin);
  EXPECT_TRUE (skinHasValue (applied, "lineGlow"));
  EXPECT_FLOAT_EQ (loadTheme (applied).lineGlow, 0.f);
}

TEST (SkinSections, ASectionOffTurnsOffEveryEffectInIt)
{
  auto skin = parse (R"({ "blobSparkle": 1, "blobBolt": 1, "blobTrail": 1 })");
  setSkinSwitch (skin, sectionSwitchPath (SkinSection::Blob), false);

  auto const theme = loadTheme (withSkinSwitchesApplied (skin));
  EXPECT_FLOAT_EQ (theme.blobSparkle, 0.f);
  EXPECT_FLOAT_EQ (theme.blobBolt, 0.f);
  EXPECT_FLOAT_EQ (theme.blobTrail, 0.f);

  // Another section is left alone.
  EXPECT_FLOAT_EQ (theme.lineGlow, Theme{}.lineGlow);
}

// The net is two fields, inside and outside the rim; off is both, or a
// filament would stop dead at the edge of the ball.
TEST (SkinSections, TheNetOffIsBothHalvesOfIt)
{
  auto skin = parse (R"({ "energy": { "netIntensity": 0.8,
                                      "netBeamIntensity": 1.5 } })");
  setSkinSwitch (skin, effectSwitchPath (SkinSection::Sphere, "net"), false);

  auto const applied = withSkinSwitchesApplied (skin);
  EXPECT_DOUBLE_EQ (skinValue (applied, "energy.netIntensity"), 0.0);
  EXPECT_DOUBLE_EQ (skinValue (applied, "energy.netBeamIntensity"), 0.0);
}

// The corona is a reach in body radii; zero would divide by it. Off is one
// body radius -- nothing reaching past the blob.
TEST (SkinSections, TheCoronaOffReachesNoFurtherThanTheBody)
{
  auto skin = parse (R"({ "blob": { "sizeMin": 0.95, "sizeMax": 3.5 } })");
  setSkinSwitch (skin, effectSwitchPath (SkinSection::Blob, "corona"), false);

  auto const applied = withSkinSwitchesApplied (skin);
  EXPECT_DOUBLE_EQ (skinValue (applied, "blob.sizeMin"), 1.0);
  EXPECT_DOUBLE_EQ (skinValue (applied, "blob.sizeMax"), 1.0);
}

// The skewed bar puts a value that ships small near the middle, not at the
// first percent of its travel.
TEST (SkinSections, TheBarsCentreIsWhereTheSpecSaysItIs)
{
  SkinTunable const thickness{ "trajectoryThickness", "Thickness", 0.001, 0.08,
                               0.006 };
  auto const range = skinTunableRange (thickness);

  EXPECT_NEAR (range.convertTo0to1 (0.006), 0.5, 1e-6);
  EXPECT_NEAR (range.convertFrom0to1 (0.0), 0.001, 1e-9);
  EXPECT_NEAR (range.convertFrom0to1 (1.0), 0.08, 1e-9);
}

// A step is a hundredth of the travel, the same distance on the bar wherever
// the value sits.
TEST (SkinSections, AStepIsAHundredthOfTheTravel)
{
  SkinTunable const glow{ "lineGlow", "Glow", 0.0, 2.0, 1.0 };

  EXPECT_NEAR (stepSkinTunable (glow, 1.0, 1), 1.02, 1e-9);
  EXPECT_NEAR (stepSkinTunable (glow, 1.0, -5), 0.9, 1e-9);
}

TEST (SkinSections, AStepStopsAtTheEndsOfTheBar)
{
  SkinTunable const glow{ "lineGlow", "Glow", 0.0, 2.0, 1.0 };

  EXPECT_DOUBLE_EQ (stepSkinTunable (glow, 1.99, 5), 2.0);
  EXPECT_DOUBLE_EQ (stepSkinTunable (glow, 0.01, -5), 0.0);
}

// A count moves by at least one, whatever a hundredth of its bar is.
TEST (SkinSections, AWholeNumberStepsByAtLeastOne)
{
  SkinTunable const count{ "speakerLight.boltCount", "Count", 1.0, 12.0, 6.0,
                           true };

  EXPECT_DOUBLE_EQ (stepSkinTunable (count, 6.0, 1), 7.0);
  EXPECT_DOUBLE_EQ (stepSkinTunable (count, 6.0, -1), 5.0);
}

// The theme is read in four places; each of them has to see a switch. So the
// theme applies them itself rather than trusting every caller to.
TEST (SkinSections, TheThemeHonoursTheSwitchesItself)
{
  auto skin = parse (R"({ "lineGlow": 0.5, "sphereGrid": 1.0 })");
  setSkinSwitch (skin, effectSwitchPath (SkinSection::Trajectory, "glow"),
                 false);
  setSkinSwitch (skin, sectionSwitchPath (SkinSection::Sphere), false);

  auto const theme = loadTheme (skin);
  EXPECT_FLOAT_EQ (theme.lineGlow, 0.f);
  EXPECT_FLOAT_EQ (theme.sphereGrid, 0.f);
}
