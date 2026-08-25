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

#include <a3-motion-ui/theme/Theme.hh>

using namespace a3;

namespace
{

// A skin file that is missing, unreadable or half written must still leave a
// usable picture. The same rule buttonLedColour already follows: fall back to
// the built-in value rather than reading absent channels as 0 and quietly
// going black.

TEST (Theme, DefaultsStandOnTheirOwn)
{
  auto const theme = loadTheme (juce::var{});

  EXPECT_GT (theme.textPrimary.r + theme.textPrimary.g + theme.textPrimary.b,
             300)
      << "text has to be readable without a skin file";
  EXPECT_LT (theme.surface.r + theme.surface.g + theme.surface.b, 200)
      << "the panel ground has to stay dark without a skin file";
}

TEST (Theme, AFileOverlaysTheDefaults)
{
  auto const parsed
      = juce::JSON::parse (R"({"accent": {"r": 10, "g": 20, "b": 30}})");
  auto const theme = loadTheme (parsed);

  EXPECT_EQ (theme.accent.r, 10);
  EXPECT_EQ (theme.accent.g, 20);
  EXPECT_EQ (theme.accent.b, 30);
}

TEST (Theme, AnEntryOnlyReplacesItsOwnRole)
{
  auto const parsed
      = juce::JSON::parse (R"({"accent": {"r": 10, "g": 20, "b": 30}})");
  auto const theme = loadTheme (parsed);
  auto const defaults = loadTheme (juce::var{});

  EXPECT_EQ (theme.textPrimary.r, defaults.textPrimary.r);
  EXPECT_EQ (theme.surface.g, defaults.surface.g);
}

TEST (Theme, APartialEntryFallsBackRatherThanGoingDark)
{
  // A missing "b" would otherwise read as 0 and turn the accent into a muddy
  // dark yellow — which looks like a rendering bug, not a config gap.
  auto const parsed = juce::JSON::parse (R"({"accent": {"r": 10, "g": 20}})");
  auto const theme = loadTheme (parsed);
  auto const defaults = loadTheme (juce::var{});

  EXPECT_EQ (theme.accent.r, defaults.accent.r);
  EXPECT_EQ (theme.accent.g, defaults.accent.g);
  EXPECT_EQ (theme.accent.b, defaults.accent.b);
}

TEST (Theme, ChannelsKeepTheirOrder)
{
  auto const parsed = juce::JSON::parse (
      R"({"channels": [{"r":1,"g":1,"b":1},{"r":2,"g":2,"b":2},)"
      R"({"r":3,"g":3,"b":3},{"r":4,"g":4,"b":4}]})");
  auto const theme = loadTheme (parsed);

  for (int i = 0; i < numThemeChannels; ++i)
    EXPECT_EQ (theme.channel[i].r, i + 1) << "channel " << i;
}

TEST (Theme, SizesAndAlphasComeFromTheSkinToo)
{
  auto const parsed = juce::JSON::parse (
      R"({"sphereScale": 0.5, "strokeThin": 2.5, "alphaDisabled": 0.25})");
  auto const theme = loadTheme (parsed);

  EXPECT_NEAR (theme.sphereScale, 0.5f, 0.001f);
  EXPECT_NEAR (theme.strokeThin, 2.5f, 0.001f);
  EXPECT_NEAR (theme.alphaDisabled, 0.25f, 0.001f);
}


// Guards against a role being added to the struct and forgotten in
// loadTheme(): every name below is fed a value no default uses, and none of
// them may still read as its default afterwards. Without this a new role would
// silently ignore the skin file.
TEST (Theme, EveryRoleIsActuallyRead)
{
  constexpr char const *roles[] = {
    "surface",     "surfaceRaised", "background",        "textPrimary",
    "textMuted",   "textOnAccent",  "accent",            "warning",
    "danger",      "sphereSurface", "sphereRim",         "sphereEnvironment",
    "boltCore",    "sphereGlow",    "speakerLight",      "energy",
  };

  juce::DynamicObject::Ptr skin{ new juce::DynamicObject{} };
  for (auto const *role : roles)
    {
      juce::DynamicObject::Ptr entry{ new juce::DynamicObject{} };
      entry->setProperty ("r", 7);
      entry->setProperty ("g", 11);
      entry->setProperty ("b", 13);
      skin->setProperty (juce::Identifier (role), entry.get ());
    }

  auto const theme = loadTheme (juce::var{ skin.get () });

  auto const wasRead = [] (ThemeColour const &c) {
    return c.r == 7 && c.g == 11 && c.b == 13;
  };

  EXPECT_TRUE (wasRead (theme.surface)) << "surface";
  EXPECT_TRUE (wasRead (theme.surfaceRaised)) << "surfaceRaised";
  EXPECT_TRUE (wasRead (theme.background)) << "background";
  EXPECT_TRUE (wasRead (theme.textPrimary)) << "textPrimary";
  EXPECT_TRUE (wasRead (theme.textMuted)) << "textMuted";
  EXPECT_TRUE (wasRead (theme.textOnAccent)) << "textOnAccent";
  EXPECT_TRUE (wasRead (theme.accent)) << "accent";
  EXPECT_TRUE (wasRead (theme.warning)) << "warning";
  EXPECT_TRUE (wasRead (theme.danger)) << "danger";
  EXPECT_TRUE (wasRead (theme.sphereSurface)) << "sphereSurface";
  EXPECT_TRUE (wasRead (theme.sphereRim)) << "sphereRim";
  EXPECT_TRUE (wasRead (theme.sphereEnvironment)) << "sphereEnvironment";
  EXPECT_TRUE (wasRead (theme.boltCore)) << "boltCore";
  EXPECT_TRUE (wasRead (theme.sphereGlow)) << "sphereGlow";
  EXPECT_TRUE (wasRead (theme.speakerLight)) << "speakerLight";
  EXPECT_TRUE (wasRead (theme.energy)) << "energy";
}

}
