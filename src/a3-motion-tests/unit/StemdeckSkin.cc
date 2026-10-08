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

// The StemDeck look as a skin (decided 2026-10-08): StemDeck's neutral greys
// and state colours, StemDeck's channel colours, and a sphere surround that stays
// dark. What the maintainer decided is held here, so a later edit of the file
// that breaks one of those decisions says so.

#include <gtest/gtest.h>

#include <JuceHeader.h>

#include <a3-motion-ui/theme/Theme.hh>
#include <a3-motion-ui/theme/ThemeColours.hh>
#include <a3-motion-ui/theme/TransportLook.hh>

using namespace a3;

namespace
{
juce::File
shippedSkinFile (char const *name)
{
  return juce::File (A3_CONFIG_JSON_PATH)
      .getParentDirectory ()
      .getChildFile ("skins")
      .getChildFile (juce::String (name) + ".json");
}

Theme
shippedTheme (char const *name)
{
  auto const file = shippedSkinFile (name);
  EXPECT_TRUE (file.existsAsFile ()) << file.getFullPathName ();
  return loadTheme (juce::JSON::parse (file));
}

/** CIE L*, from the relative luminance the contrast rules already use. */
float
lightness (ThemeColour colour)
{
  auto const y = relativeLuminance (toColour (colour));
  return y > 0.008856f ? 116.f * std::cbrt (y) - 16.f : 903.3f * y;
}

bool
sameColour (ThemeColour a, ThemeColour b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b;
}
}

TEST (StemdeckSkin, ItShipsAndLoads)
{
  EXPECT_TRUE (shippedSkinFile ("stemdeck").existsAsFile ());
}

// The sphere is additive light, and the eye goes to the brightest thing: the
// area around the ball stays at the house's deepest level (E0, L* 1-4) rather
// than StemDeck's own lighter window grey.
TEST (StemdeckSkin, TheSphereSurroundStaysDark)
{
  auto const t = shippedTheme ("stemdeck");
  EXPECT_LE (lightness (t.background), 4.f);
  EXPECT_LE (lightness (t.sphereSurface), 4.f);
  EXPECT_LE (lightness (t.sphereEnvironment), 4.f);
}

// The channels are StemDeck's own stem colours, in StemDeck's order, so a
// channel on Motion looks like the stem StemDeck colours that way. The
// maintainer's call (2026-10-08): "fake it for now" -- until the colours
// come from one place for every device. It knowingly leaves the shipped
// channel hues (the designer's within-10-degrees rule) for this skin only.
TEST (StemdeckSkin, TheChannelsAreStemDecksColours)
{
  auto const t = shippedTheme ("stemdeck");
  ThemeColour const stemDeck[numThemeChannels]
      = { { 232, 163, 61 }, { 79, 179, 191 }, { 208, 106, 134 },
          { 143, 155, 214 } };
  for (int ch = 0; ch < numThemeChannels; ++ch)
    EXPECT_TRUE (sameColour (t.channel[ch], stemDeck[ch]))
        << "channel " << ch + 1;
}

// The meter's yellow band and the accent key are `highlight`. StemDeck's own
// meter yellow is exactly the soft channel 3, so it is not taken over: the
// yellow that says "level" must not be the one that says "channel 3".
TEST (StemdeckSkin, TheHighlightIsNotChannelThree)
{
  auto const t = shippedTheme ("stemdeck");
  EXPECT_FALSE (sameColour (t.highlight, t.channel[2]));
}

// StemDeck's ladder: neutral greys, each step lighter than the one below.
TEST (StemdeckSkin, TheSurfacesClimbTheLadder)
{
  auto const t = shippedTheme ("stemdeck");
  EXPECT_LT (lightness (t.background), lightness (t.surface));
  EXPECT_LT (lightness (t.surface), lightness (t.surfaceRaised));
  EXPECT_LE (lightness (t.surfaceRaised), 25.f) << "no surface above L* 25";
}
